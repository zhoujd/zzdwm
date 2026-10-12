#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_SIZE          131072UL
#define ERROR_SIZE           256
#define MAX_HOST_FUNCTIONS   16
#define MAX_HOST_ARGS        8
#define HOST_ARG_SIZE        32

typedef int (*HostFunction)(int argc, const char *const *argv);

typedef struct Object Object;

typedef enum Type {
  TYPE_NUMBER,
  TYPE_STRING,
  TYPE_SYMBOL,
  TYPE_CONS,
  TYPE_LAMBDA,
  TYPE_MACRO,
  TYPE_PRIMITIVE,
  TYPE_HOST,
  TYPE_ENV
} Type;

struct Object {
  Type type;
  size_t size;
  union {
    struct { double number; };                      // number
    struct { char string[sizeof (Object *[3])]; };  // string, symbol
    struct { Object *car, *cdr; };                  // cons
    struct { Object *params, *body, *env; };        // lambda, macro
    struct { int primitive; const char *name; };    // primitive
    struct { HostFunction eval; const char *host_name; };// host function
    struct { Object *parent, *vars, *vals; };       // env
    struct { Object *forward; };                    // forwarding pointer
  };
};

static Object *symbols = NULL;
static Object *nil = &(Object){ TYPE_SYMBOL, .string = "nil" };
static Object *t = &(Object){ TYPE_SYMBOL, .string = "t" };

typedef struct Stream {
  const char *buffer;
  size_t length, offset;
} Stream;

typedef struct Memory {
  size_t capacity, fromOffset, toOffset;
  void *fromSpace, *toSpace;
} Memory;

static Memory *memory = &(Memory){ MEMORY_SIZE, 0, 0, NULL, NULL };

static jmp_buf exceptionEnv;

typedef struct Buffer {
  char *data;
  size_t capacity, length;
} Buffer;

static Buffer outputBuffer;
static char errorData[ERROR_SIZE];
static Buffer errorBuffer = { errorData, ERROR_SIZE, 0 };

typedef struct HostDefinition {
  const char *name;
  HostFunction eval;
} HostDefinition;

static HostDefinition hostDefinitions[MAX_HOST_FUNCTIONS];
static size_t hostDefinitionCount = 0;

static Object *environment = NULL;
static bool initialized = false;

static void bufferPutc(Buffer *buffer, char value);
static void bufferPuts(Buffer *buffer, const char *value);
static void bufferPrintf(Buffer *buffer, const char *format, ...);
static void writeObject(Object *object, bool readably, Buffer *buffer);

// EXCEPTION HANDLING /////////////////////////////////////////////////////////

#define exception(...)       exceptionWithObject(NULL, __VA_ARGS__)

#ifdef __GNUC__
void exceptionWithObject(Object *object, char *format, ...)
  __attribute__ ((noreturn, format(printf, 2, 3)));
#endif

void exceptionWithObject(Object *object, char *format, ...) {
  errorBuffer.length = 0;
  bufferPuts(&errorBuffer, "error: ");

  if (object) {
    writeObject(object, true, &errorBuffer);
    bufferPutc(&errorBuffer, ' ');
  }

  va_list args;
  va_start(args, format);
  {
    int length = vsnprintf(errorBuffer.data + errorBuffer.length,
                           errorBuffer.capacity - errorBuffer.length,
                           format, args);

    if (length > 0)
      errorBuffer.length += (size_t) length;
  }
  va_end(args);

  longjmp(exceptionEnv, 1);
}

static void
bufferPutc(Buffer *buffer, char value)
{
  if (buffer->length + 1 >= buffer->capacity)
    return;
  buffer->data[buffer->length++] = value;
  buffer->data[buffer->length] = 0;
}

static void
bufferPuts(Buffer *buffer, const char *value)
{
  size_t length = strlen(value);

  if (length >= buffer->capacity - buffer->length)
    length = buffer->capacity - buffer->length - 1;
  memcpy(buffer->data + buffer->length, value, length);
  buffer->length += length;
  buffer->data[buffer->length] = 0;
}

static void
bufferVPrintf(Buffer *buffer, const char *format, va_list args)
{
  int length = vsnprintf(buffer->data + buffer->length,
                         buffer->capacity - buffer->length,
                         format, args);

  if (length > 0)
    buffer->length += (size_t) length;
  if (buffer->length >= buffer->capacity)
    buffer->length = buffer->capacity - 1;
  buffer->data[buffer->length] = 0;
}

static void
bufferPrintf(Buffer *buffer, const char *format, ...)
{
  va_list args;

  va_start(args, format);
  bufferVPrintf(buffer, format, args);
  va_end(args);
}

// GARBAGE COLLECTION /////////////////////////////////////////////////////////

/* This implements Cheney's copying garbage collector, with which memory is
 * divided into two equal halves (semispaces): from- and to-space. From-space
 * is where new objects are allocated, whereas to-space is used during garbage
 * collection.
 *
 * When garbage collection is performed, objects that are still in use (live)
 * are copied from from-space to to-space. To-space then becomes the new
 * from-space and vice versa, thereby discarding all objects that have not
 * been copied.
 *
 * Our garbage collector takes as input a list of root objects. Objects that
 * can be reached by recursively traversing this list are considered live and
 * will be moved to to-space. When we move an object, we must also update its
 * pointer within the list to point to the objects new location in memory.
 *
 * However, this implies that our interpreter cannot use raw pointers to
 * objects in any function that might trigger garbage collection (or risk
 * causing a SEGV when accessing an object that has been moved). Instead,
 * objects must be added to the list and then only accessed through the
 * pointer inside the list.
 *
 * Thus, whenever we would have used a raw pointer to an object, we use a
 * pointer to the pointer inside the list instead:
 *
 *   function:              pointer to pointer inside list (Object **)
 *                                  |
 *                                  v
 *   list of root objects:  pointer to object (Object *)
 *                                  |
 *                                  v
 *   semispace:             object in memory
 *
 * GC_ROOTS and GC_PARAM are used to pass the list from function to function.
 *
 * GC_TRACE adds an object to the list and declares a variable which points to
 * the objects pointer inside the list.
 *
 *   GC_TRACE(gcX, X):  add object X to the list and declare Object **gcX
 *                      to point to the pointer to X inside the list.
 */

#define GC_ROOTS             gcRoots
#define GC_PARAM             Object *GC_ROOTS

#define GC_PASTE1(name, id)  name ## id
#define GC_PASTE2(name, id)  GC_PASTE1(name, id)
#define GC_UNIQUE(name)      GC_PASTE2(name, __LINE__)

#define GC_TRACE(name, init)                                                 \
  Object GC_UNIQUE(GC_ROOTS) = { TYPE_CONS, .car = init, .cdr = GC_ROOTS };  \
  Object **name = &GC_UNIQUE(GC_ROOTS).car;                                  \
  GC_ROOTS = &GC_UNIQUE(GC_ROOTS)

Object *gcMoveObject(Object *object) {
  // skip object if it is not within from-space (i.e. on the stack)
  if (object < (Object *)memory->fromSpace
    || object >= (Object *)((char *)memory->fromSpace + memory->fromOffset))
    return object;

  // if the object has already been moved, return its new location
  if (object->type == (Type)-1)
    return object->forward;

  // copy object to to-space
  Object *forward = (Object *)((char *)memory->toSpace + memory->toOffset);
  memcpy(forward, object, object->size);
  memory->toOffset += object->size;

  // mark object as moved and set forwarding pointer
  object->type = (Type)-1;
  object->forward = forward;

  return object->forward;
}

void gc(GC_PARAM) {
  memory->toOffset = 0;

  // move symbols and root objects
  symbols = gcMoveObject(symbols);

  for (Object *object = GC_ROOTS; object != nil; object = object->cdr)
    object->car = gcMoveObject(object->car);

  // iterate over objects in to-space and move all objects they reference
  for (Object *object = memory->toSpace;
    object < (Object *)((char *)memory->toSpace + memory->toOffset);
    object = (Object *)((char *)object + object->size)) {

    switch (object->type) {
      case TYPE_NUMBER:
      case TYPE_STRING:
      case TYPE_SYMBOL:
      case TYPE_PRIMITIVE:
      case TYPE_HOST:
        break;
      case TYPE_CONS:
        object->car = gcMoveObject(object->car);
        object->cdr = gcMoveObject(object->cdr);
        break;
      case TYPE_LAMBDA:
      case TYPE_MACRO:
        object->params = gcMoveObject(object->params);
        object->body = gcMoveObject(object->body);
        object->env = gcMoveObject(object->env);
        break;
      case TYPE_ENV:
        object->parent = gcMoveObject(object->parent);
        object->vars = gcMoveObject(object->vars);
        object->vals = gcMoveObject(object->vals);
        break;
    }
  }

  // swap from- and to-space
  void *swap = memory->fromSpace;
  memory->fromSpace = memory->toSpace;
  memory->toSpace = swap;
  memory->fromOffset = memory->toOffset;
}

// MEMORY MANAGEMENT //////////////////////////////////////////////////////////

size_t memoryAlign(size_t size, size_t alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

Object *memoryAllocObject(Type type, size_t size, GC_PARAM) {
  size = memoryAlign(size, sizeof (void *));

  // allocate from- and to-space
  if (!memory->fromSpace) {
    memory->fromSpace = malloc(memory->capacity * 2);
    if (memory->fromSpace == NULL)
      exception("out of memory, %lu bytes",
                (unsigned long)(memory->capacity * 2));

    memory->toSpace = (char *)memory->fromSpace + memory->capacity;
  }

  // run garbage collection if capacity exceeded
  if (memory->fromOffset + size >= memory->capacity)
    gc(GC_ROOTS);
  if (memory->fromOffset + size >= memory->capacity)
    exception("out of memory, %lu bytes", (unsigned long)size);

  // allocate object in from-space
  Object *object = (Object *)((char *)memory->fromSpace + memory->fromOffset);
  object->type = type;
  object->size = size;
  memory->fromOffset += size;

  return object;
}

// CONSTRUCTING OBJECTS ///////////////////////////////////////////////////////

Object *newObject(Type type, GC_PARAM) {
  return memoryAllocObject(type, sizeof (Object), GC_ROOTS);
}

Object *newObjectFrom(Object **from, GC_PARAM) {
  Object *object = memoryAllocObject((*from)->type, (*from)->size, GC_ROOTS);
  memcpy(object, *from, (*from)->size);
  return object;
}

Object *newNumber(double number, GC_PARAM) {
  Object *object = newObject(TYPE_NUMBER, GC_ROOTS);
  object->number = number;
  return object;
}

Object *newObjectWithString(Type type, size_t size, GC_PARAM) {
  size = (size > sizeof (((Object *)NULL)->string))
    ? size - sizeof (((Object *)NULL)->string)
    : 0;
  return memoryAllocObject(type, sizeof (Object) + size, GC_ROOTS);
}

Object *newStringWithLength(const char *string, size_t length, GC_PARAM) {
  size_t nEscapes = 0;

  for (size_t i = 1; i < length; ++i)
    if (string[i - 1] == '\\' && strchr("\\\"trn", string[i]))
      ++i, ++nEscapes;

  Object *object = newObjectWithString(TYPE_STRING,
    length - nEscapes + 1, GC_ROOTS);

  for (size_t r = 1, w = 0; r <= length; ++r) {
    if (string[r - 1] == '\\' && r < length) {
      switch (string[r]) {
      case '\\': object->string[w++] = '\\'; r++; break;
      case '"':  object->string[w++] = '"';  r++; break;
      case 't':  object->string[w++] = '\t'; r++; break;
      case 'r':  object->string[w++] = '\r'; r++; break;
      case 'n':  object->string[w++] = '\n'; r++; break;
      default:   object->string[w++] = '\\';      break;
      }
    } else
      object->string[w++] = string[r - 1];
  }

  object->string[length - nEscapes] = '\0';
  return object;
}

Object *newString(const char *string, GC_PARAM) {
  return newStringWithLength(string, strlen(string), GC_ROOTS);
}

Object *newCons(Object **car, Object **cdr, GC_PARAM) {
  Object *object = newObject(TYPE_CONS, GC_ROOTS);
  object->car = *car;
  object->cdr = *cdr;
  return object;
}

Object *newSymbolWithLength(const char *string, size_t length, GC_PARAM) {
  for (Object *object = symbols; object != nil; object = object->cdr)
    if (memcmp(object->car->string, string, length) == 0
      && object->car->string[length] == '\0')
      return object->car;

  GC_TRACE(gcObject, newObjectWithString(TYPE_SYMBOL, length + 1, GC_ROOTS));
  memcpy((*gcObject)->string, string, length);
  (*gcObject)->string[length] = '\0';

  symbols = newCons(gcObject, &symbols, GC_ROOTS);

  return *gcObject;
}

Object *newSymbol(const char *string, GC_PARAM) {
  return newSymbolWithLength(string, strlen(string), GC_ROOTS);
}

Object *newObjectWithClosure(
  Type type, Object **params, Object **body, Object **env, GC_PARAM) {
  Object *list;

  for (list = *params; list->type == TYPE_CONS; list = list->cdr) {
    if (list->car->type != TYPE_SYMBOL)
      exceptionWithObject(list->car, "is not a symbol");
    if (list->car == nil || list->car == t)
      exceptionWithObject(list->car, "cannot be used as a parameter");
  }

  if (list != nil && list->type != TYPE_SYMBOL)
    exceptionWithObject(list, "is not a symbol");

  Object *object = newObject(type, GC_ROOTS);
  object->params = *params;
  object->body = *body;
  object->env = *env;
  return object;
}

Object *newLambda(Object **params, Object **body, Object **env, GC_PARAM) {
  return newObjectWithClosure(TYPE_LAMBDA, params, body, env, GC_ROOTS);
}

Object *newMacro(Object **params, Object **body, Object **env, GC_PARAM) {
  return newObjectWithClosure(TYPE_MACRO, params, body, env, GC_ROOTS);
}

Object *newPrimitive(int primitive, const char *name, GC_PARAM) {
  Object *object = newObject(TYPE_PRIMITIVE, GC_ROOTS);
  object->primitive = primitive;
  object->name = name;
  return object;
}

Object *newHost(HostFunction eval, const char *name, GC_PARAM) {
  Object *object = newObject(TYPE_HOST, GC_ROOTS);
  object->eval = eval;
  object->host_name = name;
  return object;
}

Object *newEnv(Object **func, Object **vals, GC_PARAM) {
  Object *object = newObject(TYPE_ENV, GC_ROOTS);

  if ((*func) == nil)
    object->parent = object->vars = object->vals = nil;
  else {
    Object *param = (*func)->params, *val = *vals;

    for (int nArgs = 0;; param = param->cdr, val = val->cdr, ++nArgs) {
      if (param == nil && val == nil)
        break;
      else if (param != nil && param->type == TYPE_SYMBOL)
        break;
      else if (val != nil && val->type != TYPE_CONS)
        exceptionWithObject(val, "is not a list");
      else if (param == nil && val != nil)
        exceptionWithObject(*func, "expects at most %d arguments", nArgs);
      else if (param != nil && val == nil) {
        for (; param->type == TYPE_CONS; param = param->cdr, ++nArgs);
        exceptionWithObject(*func, "expects at least %d arguments", nArgs);
      }
    }

    object->parent = (*func)->env;
    object->vars = (*func)->params;
    object->vals = *vals;
  }

  return object;
}

// STREAM INPUT ///////////////////////////////////////////////////////////////

int streamGetc(Stream *stream) {
  if (stream->offset >= stream->length) {
    if (!stream->length && stream->buffer != NULL && *stream->buffer != 0) {
      stream->length = strlen(stream->buffer);
      return streamGetc(stream);
    }
    return EOF;
  }

  return (unsigned char)stream->buffer[stream->offset++];
}

Stream *streamSeek(Stream *stream, int offset) {
  if (offset < 0) {
    size_t backward = (size_t)(-(long)offset);

    stream->offset = backward >= stream->offset ? 0 : stream->offset - backward;
  } else
    stream->offset += (size_t)offset;
  return stream;
}

int streamPeek(Stream *stream) {
  int ch = streamGetc(stream);
  if (ch != EOF)
    streamSeek(stream, -1);
  return ch;
}

// READING S-EXPRESSIONS //////////////////////////////////////////////////////

Object *readExpr(Stream *stream, GC_PARAM);

int readNext(Stream *stream) {
  for (;;) {
    int ch = streamGetc(stream);
    if (ch == ';')
      while ((ch = streamGetc(stream)) != EOF && ch != '\n');
    if (isspace(ch))
      continue;
    return ch;
  }
}

int peekNext(Stream *stream) {
  int ch = readNext(stream);
  if (ch != EOF)
    streamSeek(stream, -1);
  return ch;
}

int readWhile(Stream *stream, int (*predicate)(int ch)) {
  for (;;) {
    int ch = streamPeek(stream);
    if (!predicate(ch))
      return ch;
    streamGetc(stream);
  }
}

Object *readUnary(Stream *stream, const char *symbol, GC_PARAM) {
  if (peekNext(stream) == EOF)
    exception("unexpected end of stream in %s", symbol);

  GC_TRACE(gcSymbol, newSymbol(symbol, GC_ROOTS));
  GC_TRACE(gcObject, readExpr(stream, GC_ROOTS));

  *gcObject = newCons(gcObject, &nil, GC_ROOTS);
  *gcObject = newCons(gcSymbol, gcObject, GC_ROOTS);

  return *gcObject;
}

Object *readString(Stream *stream, GC_PARAM) {
  size_t offset = stream->offset;

  for (bool isEscaped = false;;) {
    int ch = streamGetc(stream);
    if (ch == EOF)
      exception("unexpected end of stream in string literal \"%.*s\"",
        (int)(stream->offset - offset), stream->buffer + offset);
    if (ch == '"' && !isEscaped)
      return newStringWithLength(stream->buffer + offset,
        stream->offset - offset - 1, GC_ROOTS);

    isEscaped = (ch == '\\' && !isEscaped);
  }
}

int isSymbolChar(int ch) {
  static const char *valid = "!#$%&*+-./:<=>?@^_~";
  return isalnum(ch) || strchr(valid, ch);
}

Object *readNumberOrSymbol(Stream *stream, GC_PARAM) {
  size_t offset = stream->offset;
  int ch = streamPeek(stream);

  // skip optional leading sign
  if (ch == '+' || ch == '-') {
    streamGetc(stream);
    ch = streamPeek(stream);
  }

  // try to read a number in integer or decimal format
  if (ch == '.' || isdigit(ch)) {
    if (isdigit(ch))
      ch = readWhile(stream, isdigit);
    if (!isSymbolChar(ch))
      return newNumber(strtol(stream->buffer + offset, NULL, 10), GC_ROOTS);
    if (ch == '.') {
      ch = streamGetc(stream);
      if (isdigit(streamPeek(stream))) {
        ch = readWhile(stream, isdigit);
        if (!isSymbolChar(ch))
          return newNumber(strtod(stream->buffer + offset, NULL), GC_ROOTS);
      }
    }
  }

  // non-numeric character encountered, read a symbol
  readWhile(stream, isSymbolChar);
  return newSymbolWithLength(stream->buffer + offset,
    stream->offset - offset, GC_ROOTS);
}

Object *reverseList(Object *list) {
  Object *object = nil;

  while (list != nil) {
    Object *swap = list;
    list = list->cdr;
    swap->cdr = object;
    object = swap;
  }

  return object;
}

Object *readList(Stream *stream, GC_PARAM) {
  GC_TRACE(gcList, nil);
  GC_TRACE(gcLast, nil);

  for (;;) {
    int ch = readNext(stream);
    if (ch == EOF)
      exception("unexpected end of stream in list");
    else if (ch == ')')
      return reverseList(*gcList);
    else if (ch == '.' && !isSymbolChar(streamPeek(stream))) {
      if (*gcLast == nil)
        exception("unexpected dot at start of list");
      if ((ch = peekNext(stream)) == ')')
        exception("expected object at end of dotted list");
      if (!(*gcLast = readExpr(stream, GC_ROOTS)))
        exception("unexpected end of stream in dotted list");
      if ((ch = peekNext(stream)) != ')')
        exception("unexpected object at end of dotted list");

      readNext(stream);
      Object *list = reverseList(*gcList);
      (*gcList)->cdr = *gcLast;

      return list;
    } else {
      *gcLast = readExpr(streamSeek(stream, -1), GC_ROOTS);
      *gcList = newCons(gcLast, gcList, GC_ROOTS);
    }
  }
}

Object *readExpr(Stream *stream, GC_PARAM) {
  for (;;) {
    int ch = readNext(stream);
    if (ch == EOF)
      return NULL;
    else if (ch == '#' && streamPeek(stream) == '\'')
      {
        streamGetc(stream);
        return readUnary(stream, "function", GC_ROOTS);
      }
    else if (ch == '\'')
      return readUnary(stream, "quote", GC_ROOTS);
    else if (ch == '"')
      return readString(stream, GC_ROOTS);
    else if (ch == '(')
      return readList(stream, GC_ROOTS);
    else if (isSymbolChar(ch)
      && (ch != '.' || isSymbolChar(streamPeek(stream))))
      return readNumberOrSymbol(streamSeek(stream, -1), GC_ROOTS);
    else
      exception("unexpected character, `%c'", ch);
  }
}

// WRITING OBJECTS ////////////////////////////////////////////////////////////

void writeObject(Object *object, bool readably, Buffer *buffer) {
  switch (object->type) {
#define CASE(type, ...)                                                      \
  case type:                                                                 \
    bufferPrintf(buffer, __VA_ARGS__);                                       \
    break
  CASE(TYPE_NUMBER, "%g", object->number);
  CASE(TYPE_SYMBOL, "%s", object->string);
  CASE(TYPE_PRIMITIVE, "#<Primitive %s>", object->name);
#undef CASE
  case TYPE_STRING:
    if (readably) {
      bufferPutc(buffer, '"');
      for (char *string = object->string; *string; ++string) {
        switch (*string) {
        case '"':  bufferPuts(buffer, "\\\""); break;
        case '\t': bufferPuts(buffer, "\\t");  break;
        case '\r': bufferPuts(buffer, "\\r");  break;
        case '\n': bufferPuts(buffer, "\\n");  break;
        case '\\': bufferPuts(buffer, "\\\\"); break;
        default:   bufferPutc(buffer, *string); break;
        }
      }
      bufferPutc(buffer, '"');
    } else
      bufferPuts(buffer, object->string);
    break;
  case TYPE_CONS:
    bufferPutc(buffer, '(');
    writeObject(object->car, readably, buffer);
    while (object->cdr != nil) {
      object = object->cdr;
      if (object->type == TYPE_CONS) {
        bufferPutc(buffer, ' ');
        writeObject(object->car, readably, buffer);
      } else {
        bufferPuts(buffer, " . ");
        writeObject(object, readably, buffer);
        break;
      }
    }
    bufferPutc(buffer, ')');
    break;
#define CASE(type, name, object)                                             \
  case type:                                                                 \
    bufferPrintf(buffer, "#<%s ", name);                                     \
    writeObject(object, readably, buffer);                                   \
    bufferPutc(buffer, '>');                                                 \
    break
  CASE(TYPE_LAMBDA, "Lambda", object->params);
  CASE(TYPE_MACRO, "Macro", object->params);
  CASE(TYPE_ENV, "Env", object->vars);
#undef CASE
  case TYPE_HOST:
    bufferPrintf(buffer, "#<Function %s>", object->host_name);
    break;
  }
}

// ENVIRONMENT ////////////////////////////////////////////////////////////////

/* An environment consists of a pointer to its parent environment (if any) and
 * two parallel lists - vars and vals.
 *
 * Case 1 - vars is a regular list:
 *   vars: (a b c), vals: (1 2 3)        ; a = 1, b = 2, c = 3
 *
 * Case 2 - vars is a dotted list:
 *   vars: (a b . c), vals: (1 2)        ; a = 1, b = 2, c = nil
 *   vars: (a b . c), vals: (1 2 3)      ; a = 1, b = 2, c = (3)
 *   vars: (a b . c), vals: (1 2 3 4 5)  ; a = 1, b = 2, c = (3 4 5)
 *
 * Case 3 - vars is a symbol:
 *   vars: a, vals: nil                  ; a = nil
 *   vars: a, vals: (1)                  ; a = (1)
 *   vars: a, vals: (1 2 3)              ; a = (1 2 3)
 *
 * Case 4 - vars and vals are both nil:
 *   vars: nil, vals: nil
 */

Object *envLookup(Object *var, Object *env) {
  for (; env != nil; env = env->parent) {
    Object *vars = env->vars, *vals = env->vals;

    for (; vars->type == TYPE_CONS; vars = vars->cdr, vals = vals->cdr)
      if (vars->car == var)
        return vals->car;

    if (vars == var)
      return vals;
  }

  exceptionWithObject(var, "has no value");
}

Object *envAdd(Object **var, Object **val, Object **env, GC_PARAM) {
  GC_TRACE(gcVars, newCons(var, &nil, GC_ROOTS));
  GC_TRACE(gcVals, newCons(val, &nil, GC_ROOTS));

  (*gcVars)->cdr = (*env)->vars, (*env)->vars = *gcVars;
  (*gcVals)->cdr = (*env)->vals, (*env)->vals = *gcVals;

  return *val;
}

Object *envSet(Object **var, Object **val, Object **env, GC_PARAM) {
  GC_TRACE(gcEnv, *env);

  for (;;) {
    Object *vars = (*gcEnv)->vars, *vals = (*gcEnv)->vals;

    for (; vars->type == TYPE_CONS; vars = vars->cdr, vals = vals->cdr) {
      if (vars->car == *var)
        return vals->car = *val;
      if (vars->cdr == *var)
        return vals->cdr = *val;
    }

    if ((*gcEnv)->parent == nil)
      return envAdd(var, val, gcEnv, GC_ROOTS);
    else
      *gcEnv = (*gcEnv)->parent;
  }
}

// PRIMITIVES /////////////////////////////////////////////////////////////////

Object *primitiveAtom(Object **args, GC_PARAM) {
  return ((*args)->car->type != TYPE_CONS) ? t : nil;
}

Object *primitiveEq(Object **args, GC_PARAM) {
  Object *first = (*args)->car, *second = (*args)->cdr->car;

  if (first->type == TYPE_NUMBER && second->type == TYPE_NUMBER)
    return (first->number == second->number) ? t : nil;
  else if (first->type == TYPE_STRING && second->type == TYPE_STRING)
    return !strcmp(first->string, second->string) ? t : nil;
  else
    return (first == second) ? t : nil;
}

Object *primitiveCar(Object **args, GC_PARAM) {
  Object *first = (*args)->car;

  if (first == nil)
    return nil;
  else if (first->type == TYPE_CONS)
    return first->car;
  else
    exceptionWithObject(first, "is not a list");
}

Object *primitiveCdr(Object **args, GC_PARAM) {
  Object *first = (*args)->car;

  if (first == nil)
    return nil;
  else if (first->type == TYPE_CONS)
    return first->cdr;
  else
    exceptionWithObject(first, "is not a list");
}

Object *primitiveCons(Object **args, GC_PARAM) {
  GC_TRACE(gcFirst, (*args)->car);
  GC_TRACE(gcSecond, (*args)->cdr->car);

  return newCons(gcFirst, gcSecond, GC_ROOTS);
}

Object *primitivePrint(Object **args, GC_PARAM) {
  bufferPutc(&outputBuffer, '\n');
  writeObject((*args)->car, true, &outputBuffer);
  bufferPutc(&outputBuffer, ' ');
  return (*args)->car;
}

Object *primitivePrinc(Object **args, GC_PARAM) {
  writeObject((*args)->car, false, &outputBuffer);
  return (*args)->car;
}

#define DEFINE_PRIMITIVE_ARITHMETIC(name, op, init)                          \
Object *name(Object **args, GC_PARAM) {                                      \
  if (*args == nil)                                                          \
    return newNumber(init, GC_ROOTS);                                        \
  else if ((*args)->car->type != TYPE_NUMBER)                                \
    exceptionWithObject((*args)->car, "is not a number");                    \
  else {                                                                     \
    Object *object, *rest;                                                   \
                                                                             \
    if ((*args)->cdr == nil) {                                               \
      object = newNumber(init, GC_ROOTS);                                    \
      rest = *args;                                                          \
    } else {                                                                 \
      GC_TRACE(gcFirst, (*args)->car);                                       \
      object = newObjectFrom(gcFirst, GC_ROOTS);                             \
      rest = (*args)->cdr;                                                   \
    }                                                                        \
                                                                             \
    for (; rest != nil; rest = rest->cdr) {                                  \
      if (rest->car->type != TYPE_NUMBER)                                    \
        exceptionWithObject(rest->car, "is not a number");                   \
                                                                             \
      object->number = object->number op rest->car->number;                  \
    }                                                                        \
                                                                             \
    return object;                                                           \
  }                                                                          \
}

DEFINE_PRIMITIVE_ARITHMETIC(primitiveAdd,      +, 0)
DEFINE_PRIMITIVE_ARITHMETIC(primitiveSubtract, -, 0)
DEFINE_PRIMITIVE_ARITHMETIC(primitiveMultiply, *, 1)
DEFINE_PRIMITIVE_ARITHMETIC(primitiveDivide,   /, 1)

#define DEFINE_PRIMITIVE_RELATIONAL(name, op)                                \
Object *name(Object **args, GC_PARAM) {                                      \
  if ((*args)->car->type != TYPE_NUMBER)                                     \
    exceptionWithObject((*args)->car, "is not a number");                    \
  else {                                                                     \
    Object *rest = *args;                                                    \
    bool result = true;                                                      \
                                                                             \
    for (; result && rest->cdr != nil; rest = rest->cdr) {                   \
      if (rest->cdr->car->type != TYPE_NUMBER)                               \
        exceptionWithObject(rest->cdr->car, "is not a number");              \
                                                                             \
      result &= rest->car->number op rest->cdr->car->number;                 \
    }                                                                        \
                                                                             \
    return result ? t : nil;                                                 \
  }                                                                          \
}

DEFINE_PRIMITIVE_RELATIONAL(primitiveEqual,        ==)
DEFINE_PRIMITIVE_RELATIONAL(primitiveLess,         < )
DEFINE_PRIMITIVE_RELATIONAL(primitiveLessEqual,    <=)
DEFINE_PRIMITIVE_RELATIONAL(primitiveGreater,      > )
DEFINE_PRIMITIVE_RELATIONAL(primitiveGreaterEqual, >=)

typedef struct Primitive {
  const char *name;
  int nMinArgs, nMaxArgs;
  Object *(* eval)(Object **args, GC_PARAM);
} Primitive;

Primitive primitives[] = {
  { "quote",  1,  1, NULL /* special form */ },
  { "function", 1, 1, NULL /* special form */ },
  { "setq",   0, -2, NULL /* special form */ },
  { "progn",  0, -1, NULL /* special form */ },
  { "if",     2,  3, NULL /* special form */ },
  { "cond",   0, -1, NULL /* special form */ },
  { "lambda", 1, -1, NULL /* special form */ },
  { "macro",  1, -1, NULL /* special form */ },
  { "atom",   1,  1, primitiveAtom         },
  { "eq",     2,  2, primitiveEq           },
  { "car",    1,  1, primitiveCar          },
  { "cdr",    1,  1, primitiveCdr          },
  { "cons",   2,  2, primitiveCons         },
  { "print",  1,  1, primitivePrint        },
  { "princ",  1,  1, primitivePrinc        },
  { "+",      0, -1, primitiveAdd          },
  { "-",      1, -1, primitiveSubtract     },
  { "*",      0, -1, primitiveMultiply     },
  { "/",      1, -1, primitiveDivide       },
  { "=",      1, -1, primitiveEqual        },
  { "<",      1, -1, primitiveLess         },
  { "<=",     1, -1, primitiveLessEqual    },
  { ">",      1, -1, primitiveGreater      },
  { ">=",     1, -1, primitiveGreaterEqual }
};

// Special forms handled by evalExpr. Must be in the same order as above.
enum {
  PRIMITIVE_QUOTE,
  PRIMITIVE_FUNCTION,
  PRIMITIVE_SETQ,
  PRIMITIVE_PROGN,
  PRIMITIVE_IF,
  PRIMITIVE_COND,
  PRIMITIVE_LAMBDA,
  PRIMITIVE_MACRO
};

// EVALUATION /////////////////////////////////////////////////////////////////

/* Scheme-style tail recursive evaluation. evalProgn, evalIf and evalCond
 * return the object in the tail recursive position to be evaluated by
 * evalExpr. Macros are expanded in-place the first time they are evaluated.
 */

Object *evalExpr(Object **object, Object **env, GC_PARAM);

Object *evalSetq(Object **args, Object **env, GC_PARAM) {
  if (*args == nil)
    return nil;
  else {
    GC_TRACE(gcVar, (*args)->car);
    GC_TRACE(gcVal, (*args)->cdr->car);

    if ((*gcVar)->type != TYPE_SYMBOL)
      exceptionWithObject(*gcVar, "is not a symbol");
    if (*gcVar == nil || *gcVar == t)
      exceptionWithObject(*gcVar, "is a constant and cannot be set");

    *gcVal = evalExpr(gcVal, env, GC_ROOTS);
    envSet(gcVar, gcVal, env, GC_ROOTS);

    if ((*args)->cdr->cdr == nil)
      return *gcVal;
    else {
      GC_TRACE(gcArgs, (*args)->cdr->cdr);
      return evalSetq(gcArgs, env, GC_ROOTS);
    }
  }
}

Object *evalProgn(Object **args, Object **env, GC_PARAM) {
  if (*args == nil)
    return nil;
  else if ((*args)->cdr == nil)
    return (*args)->car;
  else {
    GC_TRACE(gcObject, (*args)->car);
    GC_TRACE(gcArgs, (*args)->cdr);

    evalExpr(gcObject, env, GC_ROOTS);
    return evalProgn(gcArgs, env, GC_ROOTS);
  }
}

Object *evalIf(Object **args, Object **env, GC_PARAM) {
  GC_TRACE(gcObject, (*args)->car);

  if (evalExpr(gcObject, env, GC_ROOTS) != nil)
    return (*args)->cdr->car;
  else if ((*args)->cdr->cdr != nil)
    return (*args)->cdr->cdr->car;
  else
    return nil;
}

Object *evalCond(Object **args, Object **env, GC_PARAM) {
  if (*args == nil)
    return nil;
  else if ((*args)->car->type != TYPE_CONS)
    exceptionWithObject((*args)->car, "is not a list");
  else {
    GC_TRACE(gcCar, (*args)->car->car);
    GC_TRACE(gcCdr, (*args)->car->cdr);

    if ((*gcCar = evalExpr(gcCar, env, GC_ROOTS)) != nil)
      return (*gcCdr != nil) ? evalProgn(gcCdr, env, GC_ROOTS) : *gcCar;
    else {
      GC_TRACE(gcArgs, (*args)->cdr);
      return evalCond(gcArgs, env, GC_ROOTS);
    }
  }
}

Object *evalLambda(Object **args, Object **env, GC_PARAM) {
  GC_TRACE(gcParams, (*args)->car);
  GC_TRACE(gcBody, (*args)->cdr);

  return newLambda(gcParams, gcBody, env, GC_ROOTS);
}

Object *evalMacro(Object **args, Object **env, GC_PARAM) {
  GC_TRACE(gcParams, (*args)->car);
  GC_TRACE(gcBody, (*args)->cdr);

  return newMacro(gcParams, gcBody, env, GC_ROOTS);
}

Object *expandMacro(Object **macro, Object **args, GC_PARAM) {
  GC_TRACE(gcEnv, newEnv(macro, args, GC_ROOTS));
  GC_TRACE(gcBody, (*macro)->body);

  GC_TRACE(gcObject, evalProgn(gcBody, gcEnv, GC_ROOTS));
  *gcObject = evalExpr(gcObject, gcEnv, GC_ROOTS);

  return *gcObject;
}

Object *expandMacroTo(Object **macro, Object **args, Object **cons, GC_PARAM) {
  GC_TRACE(gcObject, expandMacro(macro, args, GC_ROOTS));

  if ((*gcObject)->type == TYPE_CONS) {
    (*cons)->car = (*gcObject)->car;
    (*cons)->cdr = (*gcObject)->cdr;
  } else {
    (*cons)->car = newSymbol("progn", GC_ROOTS);
    (*cons)->cdr = newCons(gcObject, &nil, GC_ROOTS);
  }

  return *cons;
}

Object *evalList(Object **args, Object **env, GC_PARAM) {
  if ((*args)->type != TYPE_CONS)
    return evalExpr(args, env, GC_ROOTS);
  else {
    GC_TRACE(gcObject, (*args)->car);
    GC_TRACE(gcArgs, (*args)->cdr);

    *gcObject = evalExpr(gcObject, env, GC_ROOTS);
    *gcArgs = evalList(gcArgs, env, GC_ROOTS);

    return newCons(gcObject, gcArgs, GC_ROOTS);
  }
}

Object *callHost(Object **function, Object **args, GC_PARAM) {
  const char *argv[MAX_HOST_ARGS];
  char numbers[MAX_HOST_ARGS][HOST_ARG_SIZE];
  HostFunction eval = (*function)->eval;
  int argc = 0;

  (void) gcRoots;

  for (Object *object = *args; object != nil; object = object->cdr, argc++) {
    if (argc == MAX_HOST_ARGS)
      exceptionWithObject(*function, "expects at most %d arguments",
                          MAX_HOST_ARGS);
    if (object->type != TYPE_CONS)
      exceptionWithObject(object, "is not a list");

    if (object->car->type == TYPE_NUMBER) {
      snprintf(numbers[argc], HOST_ARG_SIZE, "%g", object->car->number);
      argv[argc] = numbers[argc];
    } else if (object->car->type == TYPE_STRING
               || object->car->type == TYPE_SYMBOL)
      argv[argc] = object->car->string;
    else
      exceptionWithObject(object->car, "is not a number or string");
  }

  if (!eval(argc, argv))
    exception("host command failed");
  return t;
}

Object *evalExpr(Object **object, Object **env, GC_PARAM) {
  GC_TRACE(gcObject, *object);
  GC_TRACE(gcEnv, *env);

  GC_TRACE(gcFunc, nil);
  GC_TRACE(gcArgs, nil);
  GC_TRACE(gcBody, nil);

  for (;;) {
    if ((*gcObject)->type == TYPE_SYMBOL)
      return envLookup(*gcObject, *gcEnv);
    if ((*gcObject)->type != TYPE_CONS)
      return *gcObject;

    *gcFunc = (*gcObject)->car;
    *gcArgs = (*gcObject)->cdr;

    *gcFunc = evalExpr(gcFunc, gcEnv, GC_ROOTS);
    *gcBody = nil;

    if ((*gcFunc)->type == TYPE_LAMBDA) {
      *gcBody = (*gcFunc)->body;
      *gcArgs = evalList(gcArgs, gcEnv, GC_ROOTS);
      *gcEnv = newEnv(gcFunc, gcArgs, GC_ROOTS);
      *gcObject = evalProgn(gcBody, gcEnv, GC_ROOTS);
    } else if ((*gcFunc)->type == TYPE_MACRO) {
      *gcObject = expandMacroTo(gcFunc, gcArgs, gcObject, GC_ROOTS);
    } else if ((*gcFunc)->type == TYPE_HOST) {
      *gcArgs = evalList(gcArgs, gcEnv, GC_ROOTS);
      return callHost(gcFunc, gcArgs, GC_ROOTS);
    } else if ((*gcFunc)->type == TYPE_PRIMITIVE) {
      Primitive *primitive = &primitives[(*gcFunc)->primitive];
      int nArgs = 0;

      for (Object *args = *gcArgs; args != nil; args = args->cdr, nArgs++)
        if (args->type != TYPE_CONS)
          exceptionWithObject(args, "is not a list");

      if (nArgs < primitive->nMinArgs)
        exceptionWithObject(*gcFunc, "expects at least %d arguments",
          primitive->nMinArgs);
      if (nArgs > primitive->nMaxArgs && primitive->nMaxArgs >= 0)
        exceptionWithObject(*gcFunc, "expects at most %d arguments",
          primitive->nMaxArgs);
      if (primitive->nMaxArgs < 0 && nArgs % -primitive->nMaxArgs)
        exceptionWithObject(*gcFunc, "expects a multiple of %d arguments",
          -primitive->nMaxArgs);

      switch ((*gcFunc)->primitive) {
      case PRIMITIVE_QUOTE:  return (*gcArgs)->car;
      case PRIMITIVE_FUNCTION: return (*gcArgs)->car;
      case PRIMITIVE_SETQ:   return evalSetq(gcArgs, gcEnv, GC_ROOTS);
      case PRIMITIVE_PROGN:  *gcObject = evalProgn(gcArgs, gcEnv, GC_ROOTS);
                             break;
      case PRIMITIVE_IF:     *gcObject = evalIf(gcArgs, gcEnv, GC_ROOTS);
                             break;
      case PRIMITIVE_COND:   *gcObject = evalCond(gcArgs, gcEnv, GC_ROOTS);
                             break;
      case PRIMITIVE_LAMBDA: return evalLambda(gcArgs, gcEnv, GC_ROOTS);
      case PRIMITIVE_MACRO:  return evalMacro(gcArgs, gcEnv, GC_ROOTS);
      default:               *gcArgs = evalList(gcArgs, gcEnv, GC_ROOTS);
                             return primitive->eval(gcArgs, GC_ROOTS);
      }
    } else
      exceptionWithObject(*gcFunc, "is not a function");
  }
}

// STANDARD LIBRARY ///////////////////////////////////////////////////////////

#define TINY_LISP_SOURCE(...) #__VA_ARGS__

static const char *stdlib = TINY_LISP_SOURCE(
  (setq list (lambda args args))

  (setq defmacro (macro (name params . body)
    (list (quote setq) name (list (quote macro) params . body))))

  (defmacro defun (name params . body)
    (list (quote setq) name (list (quote lambda) params . body)))

  (defun null (x) (eq x nil))

  (defun map1 (func xs)
    (if (null xs)
        nil
        (cons (func (car xs))
              (map1 func (cdr xs)))))

  (defmacro and args
    (cond ((null args) t)
          ((null (cdr args)) (car args))
          (t (list (quote if) (car args) (cons (quote and) (cdr args))))))

  (defmacro or args
    (if (null args)
        nil
        (cons (quote cond) (map1 list args))))

  (defun not (x) (if x nil t))

  (defun consp (x) (not (atom x)))
  (defun listp (x) (or (null x) (consp x)))

  (defun zerop (x) (= x 0))

  (defun equal (x y)
    (or (and (atom x) (atom y)
             (eq x y))
        (and (not (atom x)) (not (atom y))
             (equal (car x) (car y))
             (equal (cdr x) (cdr y)))))

  (defun nth (n xs)
    (if (zerop n)
        (car xs)
        (nth (- n 1) (cdr xs))))

  (defun append (xs y)
    (if (null xs)
        y
        (cons (car xs) (append (cdr xs) y))))
);

// MAIN ///////////////////////////////////////////////////////////////////////

Object *newRootEnv(GC_PARAM) {
  GC_TRACE(gcEnv, newEnv(&nil, &nil, GC_ROOTS));
  GC_TRACE(gcVar, nil);
  GC_TRACE(gcVal, nil);

  // add constants
  envSet(&nil, &nil, gcEnv, GC_ROOTS);
  envSet(&t, &t, gcEnv, GC_ROOTS);

  // add primitives
  int nPrimitives = sizeof (primitives) / sizeof (primitives[0]);

  for (int i = 0; i < nPrimitives; ++i) {
    *gcVar = newSymbol(primitives[i].name, GC_ROOTS);
    *gcVal = newPrimitive(i, primitives[i].name, GC_ROOTS);

    envSet(gcVar, gcVal, gcEnv, GC_ROOTS);
  }

  for (size_t i = 0; i < hostDefinitionCount; ++i) {
    *gcVar = newSymbol(hostDefinitions[i].name, GC_ROOTS);
    *gcVal = newHost(hostDefinitions[i].eval,
                     hostDefinitions[i].name, GC_ROOTS);
    envSet(gcVar, gcVal, gcEnv, GC_ROOTS);
  }

  // add standard library
  Stream stream = { stdlib, strlen(stdlib), 0 };
  GC_TRACE(gcObject, nil);

  while (peekNext(&stream) != EOF) {
    *gcObject = nil;
    *gcObject = readExpr(&stream, GC_ROOTS);
    evalExpr(gcObject, gcEnv, GC_ROOTS);
  }

  return *gcEnv;
}

int
tiny_lisp_define_host(const char *name, HostFunction function)
{
  size_t i;

  if (name == NULL || *name == 0 || function == NULL)
    return 0;
  for (i = 0; i < hostDefinitionCount; ++i)
    if (strcmp(hostDefinitions[i].name, name) == 0) {
      hostDefinitions[i].eval = function;
      return 1;
    }
  if (hostDefinitionCount == MAX_HOST_FUNCTIONS)
    return 0;
  hostDefinitions[hostDefinitionCount].name = name;
  hostDefinitions[hostDefinitionCount].eval = function;
  ++hostDefinitionCount;
  return 1;
}

static int
eval_source(const char *source, char *output, size_t output_size,
            bool single_expression)
{
  Object *gcRoots = nil;
  Object envRoot = { TYPE_CONS, .car = NULL, .cdr = gcRoots };
  Object **gcEnv = &envRoot.car;
  Stream stream;

  if (source == NULL || output == NULL || output_size == 0)
    return 0;

  outputBuffer.data = output;
  outputBuffer.capacity = output_size;
  outputBuffer.length = 0;
  output[0] = 0;

  if (setjmp(exceptionEnv)) {
    if (initialized)
      environment = *gcEnv;
    outputBuffer.length = 0;
    bufferPuts(&outputBuffer, errorBuffer.data);
    return 0;
  }

  gcRoots = &envRoot;
  if (!initialized) {
    symbols = nil;
    symbols = newCons(&nil, &symbols, gcRoots);
    symbols = newCons(&t, &symbols, gcRoots);
    envRoot.car = newRootEnv(gcRoots);
    environment = envRoot.car;
    initialized = true;
  } else {
    envRoot.car = environment;
  }

  stream.buffer = source;
  stream.length = strlen(source);
  stream.offset = 0;

  GC_TRACE(gcObject, nil);
  if (peekNext(&stream) == EOF)
    {
      if (single_expression)
        exception("empty expression");
      *gcObject = nil;
    }
  else
    while (peekNext(&stream) != EOF)
      {
        *gcObject = readExpr(&stream, gcRoots);
        if (single_expression && peekNext(&stream) != EOF)
          exception("unexpected data after expression");
        *gcObject = evalExpr(gcObject, gcEnv, gcRoots);
        environment = *gcEnv;
      }

  writeObject(*gcObject, true, &outputBuffer);
  return 1;
}

int
tiny_lisp_eval(const char *expression, char *output, size_t output_size)
{
  return eval_source(expression, output, output_size, true);
}

int
tiny_lisp_eval_source(const char *source, char *output, size_t output_size)
{
  return eval_source(source, output, output_size, false);
}
