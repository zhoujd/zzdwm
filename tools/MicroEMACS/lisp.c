/*
 * Filename lisp.c
 */

#include "def.h"
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define TYPE_SYMBOL 1
#define TYPE_STRING 2
#define TYPE_LIST   3

typedef struct LispNode
{
  int type;
  char *val;
  struct LispNode *list;
  struct LispNode *next;
} LispNode;

static void free_sexpr (LispNode *node);

static char *
lisp_strndup (const char *text, size_t length)
{
  size_t used = 0;
  char *copy;

  while (used < length && text[used] != 0)
    ++used;
  copy = malloc (used + 1);
  if (copy == NULL)
    return NULL;
  memcpy (copy, text, used);
  copy[used] = 0;
  return copy;
}

/* Parse one S-expression and advance the source pointer. */
static LispNode *
parse_sexpr (const char **src)
{
  LispNode *node;
  LispNode **current;
  const char *start;
  size_t length;

  while (**src != 0 && isspace ((uchar) **src))
    ++*src;
  if (**src == 0)
    return NULL;

  node = calloc (1, sizeof (LispNode));
  if (node == NULL)
    return NULL;

  if (**src == '(')
    {
      ++*src;
      node->type = TYPE_LIST;
      current = &node->list;
      while (**src != 0 && **src != ')')
        {
          LispNode *child = parse_sexpr (src);

          if (child == NULL)
            {
              free_sexpr (node);
              return NULL;
            }
          *current = child;
          current = &child->next;
        }
      if (**src != ')')
        {
          free_sexpr (node);
          return NULL;
        }
      ++*src;
    }
  else if (**src == '"')
    {
      ++*src;
      start = *src;
      while (**src != 0 && **src != '"')
        ++*src;
      if (**src != '"')
        {
          free (node);
          return NULL;
        }
      length = *src - start;
      node->type = TYPE_STRING;
      node->val = lisp_strndup (start, length);
      ++*src;
    }
  else
    {
      start = *src;
      while (**src != 0 && !isspace ((uchar) **src) && **src != '('
             && **src != ')' && **src != '"')
        ++*src;
      length = *src - start;
      if (length == 0)
        {
          free (node);
          return NULL;
        }
      node->type = TYPE_SYMBOL;
      node->val = lisp_strndup (start, length);
    }
  if (node->type != TYPE_LIST && node->val == NULL)
    {
      free (node);
      return NULL;
    }
  return node;
}

/* Convert a MicroEMACS key name to its internal key code. */
static int
lisp_key_code (const char *name)
{
  int key = 0;

  while (*name != 0)
    {
      if (strncasecmp (name, "C-X ", 4) == 0)
        {
          key |= KCTLX;
          name += 4;
          continue;
        }
      if (strncasecmp (name, "M-", 2) == 0)
        {
          key |= KMETA;
          name += 2;
          continue;
        }
      if (strncasecmp (name, "C-", 2) == 0)
        {
          key |= KCTRL;
          name += 2;
          continue;
        }
      break;
    }
  if (name[0] == 0 || name[1] != 0)
    return -1;
  if (name[0] >= 'a' && name[0] <= 'z')
    key |= name[0] - 'a' + 'A';
  else
    key |= name[0];
  return key;
}

static int
lisp_bind_key (const char *key_name, const char *command)
{
  SYMBOL *symbol;
  int key = lisp_key_code (key_name);

  if (key < 0 || (symbol = symlookup (command)) == NULL)
    return FALSE;
  setbinding (key, symbol);
  return TRUE;
}

static int
lisp_set_variable (const char *name, const char *value_text)
{
  char *end;
  long value;

  value = strtol (value_text, &end, 10);
  if (*value_text == 0 || *end != 0 || value <= 0 || value > INT_MAX)
    return FALSE;

  value = (int) value;
  if (strcmp (name, "tab-size") == 0 || strcmp (name, "tab-width") == 0)
    tabsize = value;
  else if (strcmp (name, "fill-column") == 0)
    fillcol = value;
  else
    return FALSE;
  return TRUE;
}

static int
eval_sexpr (const LispNode *node)
{
  const LispNode *func;
  const LispNode *first;
  const LispNode *second;

  if (!node || node->type != TYPE_LIST || !node->list)
    return FALSE;

  func = node->list;
  first = func->next;
  second = first != NULL ? first->next : NULL;
  if (func->type == TYPE_SYMBOL)
    {
      /* (define-key "C-x C-b" "buffer-menu") */
      if (strcmp (func->val, "define-key") == 0)
        {
          if (first == NULL || second == NULL || second->next != NULL
              || first->type != TYPE_STRING || second->type != TYPE_STRING)
            return FALSE;
          return lisp_bind_key (first->val, second->val);
        }
      /* (set-variable "tab-width" "8") */
      else if (strcmp (func->val, "set-variable") == 0)
        {
          if (first == NULL || second == NULL || second->next != NULL
              || first->type != TYPE_STRING || second->type != TYPE_STRING)
            return FALSE;
          return lisp_set_variable (first->val, second->val);
        }
    }
  return FALSE;
}

/* Simple Cleanup Loop */
static void
free_sexpr (LispNode *node)
{
  if (!node)
    return;
  if (node->type == TYPE_LIST)
    free_sexpr (node->list);
  free_sexpr (node->next);
  if (node->val)
    free (node->val);
  free (node);
}

int
lispeval (int f, int n, int k)
{
  char expr[128];
  const char *scan = expr;
  LispNode *node;
  int status;

  (void) f;
  (void) n;
  (void) k;

  if (ereadv ("Lisp: ", expr, sizeof (expr), EFNEW | EFCR) != TRUE)
    return FALSE;
  node = parse_sexpr (&scan);
  status = node != NULL;
  if (status == TRUE)
    {
      while (*scan != 0 && isspace ((uchar) *scan))
        ++scan;
      status = *scan == 0 && eval_sexpr (node) == TRUE;
    }
  free_sexpr (node);
  if (status == FALSE)
    eprintf ("[Lisp expression failed]");
  return status;
}
