/*
 * Filename lisp.c
 */

#include "def.h"
#include "tinylisp.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef LISP

#define LISP_EXPRESSION_SIZE 256
#define LISP_OUTPUT_SIZE     256
#define LISP_COMMAND_OVERHEAD 13
#if defined(MINGW) || defined(DOS)
#define LISP_CONFIG_FILE     "me.l"
#else
#define LISP_CONFIG_FILE     ".mel"
#endif

static int lisp_message_displayed;
static int lisp_debug_level;

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
lisp_bind_key (int argc, const char *const *argv,
               char *result, size_t result_size)
{
  SYMBOL *symbol;
  int key;

  (void) result;
  (void) result_size;
  if (argc != 2)
    return FALSE;
  key = lisp_key_code (argv[0]);
  if (key < 0 || (symbol = symlookup (argv[1])) == NULL)
    return FALSE;
  setbinding (key, symbol);
  return TRUE;
}

static int
lisp_integer (const char *text, int *value)
{
  char *end;
  long parsed;

  if (text == NULL || text[0] == 0)
    return FALSE;
  parsed = strtol (text, &end, 10);
  if (*end != 0 || parsed < INT_MIN || parsed > INT_MAX)
    return FALSE;
  *value = (int) parsed;
  return TRUE;
}

static int
lisp_execute_command (int argc, const char *const *argv,
                      char *result, size_t result_size)
{
  SYMBOL *symbol;
  int count = 1;
  int explicit_count;

  (void) result;
  (void) result_size;
  if (argc < 1 || argc > 2 || curwp == NULL)
    return FALSE;
  explicit_count = argc == 2;
  if (explicit_count && lisp_integer (argv[1], &count) == FALSE)
    return FALSE;
  if ((symbol = symlookup (argv[0])) == NULL)
    return FALSE;
  if (symbol->s_macro != NULL && (kbdmip != NULL || kbdmop != NULL))
    return FALSE;

  startsaveundo ();
  if (symbol->s_macro != NULL)
    count = domacro (symbol->s_macro, count);
  else if (symbol->s_funcp != NULL)
    count = (*symbol->s_funcp) (explicit_count, count, KRANDOM);
  else
    count = FALSE;
  endsaveundo ();
  return count;
}

static int
lisp_insert_once (const char *text)
{
  const char *start = text;

  for (;;)
    {
      const char *newline = strchr (start, '\n');
      size_t length = newline != NULL
                      ? (size_t) (newline - start) : strlen (start);

      if (length != 0
          && linsert ((int) length, 0, (char *) start) == FALSE)
        return FALSE;
      if (newline == NULL)
        return TRUE;
      if (lnewline () == FALSE)
        return FALSE;
      start = newline + 1;
    }
}

static int
lisp_insert_string (int argc, const char *const *argv,
                    char *result, size_t result_size)
{
  int count = 1;
  int index;

  (void) result;
  (void) result_size;
  if (argc < 1 || argc > 2 || curwp == NULL)
    return FALSE;
  if (argc == 2
      && (lisp_integer (argv[1], &count) == FALSE || count <= 0))
    return FALSE;
  startsaveundo ();
  for (index = 0; index < count; index++)
    {
      if (lisp_insert_once (argv[0]) == FALSE)
        {
          endsaveundo ();
          return FALSE;
        }
    }
  endsaveundo ();
  return TRUE;
}

static int
lisp_goto_bol (int argc, const char *const *argv,
               char *result, size_t result_size)
{
  (void) argv;
  (void) result;
  (void) result_size;
  if (argc != 0 || curwp == NULL)
    return FALSE;
  return gotobol (FALSE, 1, KRANDOM);
}

static int
lisp_goto_eol (int argc, const char *const *argv,
               char *result, size_t result_size)
{
  (void) argv;
  (void) result;
  (void) result_size;
  if (argc != 0 || curwp == NULL)
    return FALSE;
  return gotoeol (FALSE, 1, KRANDOM);
}

static int
lisp_forward_line (int argc, const char *const *argv,
                   char *result, size_t result_size)
{
  int count = 1;
  int status;

  (void) result;
  (void) result_size;
  if (argc > 1 || curwp == NULL)
    return FALSE;
  if (argc == 1 && lisp_integer (argv[0], &count) == FALSE)
    return FALSE;
  status = forwline (argc == 1, count, KRANDOM);
  return status == TRUE ? TRUE : TINY_LISP_HOST_NIL;
}

static int
lisp_eobp (int argc, const char *const *argv,
           char *result, size_t result_size)
{
  (void) argv;
  (void) result;
  (void) result_size;

  if (argc != 0 || curwp == NULL || curbp == NULL)
    return FALSE;
  return curwp->w_dot.p == lastline (curbp)
           ? TRUE : TINY_LISP_HOST_NIL;
}

static int
lisp_get_char (int argc, const char *const *argv,
               char *result, size_t result_size)
{
  LINE *line;
  const uchar *text;
  int length;

  (void) argv;
  if (argc != 0 || curwp == NULL || curbp == NULL)
    return FALSE;
  line = curwp->w_dot.p;
  if (line == curbp->b_linep
      || curwp->w_dot.o >= wllength (line))
    return TINY_LISP_HOST_NIL;

  text = wlgetcptr (line, curwp->w_dot.o);
  length = uclen (text);
  if (length <= 0 || (size_t) length >= result_size)
    return FALSE;
  memcpy (result, text, (size_t) length);
  result[length] = 0;
  return TINY_LISP_HOST_STRING;
}

static int
lisp_get_line (int argc, const char *const *argv,
               char *result, size_t result_size)
{
  LINE *line;
  int offset = 0;
  int length;

  if (argc > 1 || curwp == NULL || curbp == NULL)
    return FALSE;
  if (argc == 1 && lisp_integer (argv[0], &offset) == FALSE)
    return FALSE;
  line = curwp->w_dot.p;
  while (offset > 0 && line != curbp->b_linep)
    {
      line = lforw (line);
      offset--;
    }
  while (offset < 0 && line != curbp->b_linep)
    {
      line = lback (line);
      offset++;
    }
  if (offset != 0)
    return TINY_LISP_HOST_NIL;
  if (line == curbp->b_linep)
    return TINY_LISP_HOST_NIL;

  length = llength (line);
  if (length < 0 || (size_t) length >= result_size)
    return FALSE;
  memcpy (result, lgets (line), (size_t) length);
  result[length] = 0;
  return TINY_LISP_HOST_STRING;
}

static int
lisp_insert_char (int argc, const char *const *argv,
                  char *result, size_t result_size)
{
  int character;
  int length;
  int status;

  (void) result;
  (void) result_size;
  if (argc != 1 || curwp == NULL)
    return FALSE;

  startsaveundo ();
  if (lisp_integer (argv[0], &character) == TRUE)
    {
      if (character <= 0 || character > 0x10ffff)
        {
          endsaveundo ();
          return FALSE;
        }
      status = linsert (1, character, NULL);
    }
  else
    {
      length = (int) strlen (argv[0]);
      if (length <= 0 || strchr (argv[0], '\n') != NULL
          || unslen ((const uchar *) argv[0], length) != 1)
        {
          endsaveundo ();
          return FALSE;
        }
      status = linsert (length, 0, (char *) argv[0]);
    }
  if (status == FALSE)
    {
      endsaveundo ();
      return FALSE;
    }
  endsaveundo ();
  return TRUE;
}

static int
lisp_save_buffer (int argc, const char *const *argv,
                  char *result, size_t result_size)
{
  (void) argv;
  (void) result;
  (void) result_size;

  if (argc != 0)
    return FALSE;
  return filesave (FALSE, 1, KRANDOM);
}

static int
lisp_kill_emacs (int argc, const char *const *argv,
                 char *result, size_t result_size)
{
  (void) argv;
  (void) result;
  (void) result_size;

  if (argc != 0)
    return FALSE;
  return quit (TRUE, 1, KRANDOM);
}

static int
lisp_define_command (int argc, const char *const *argv,
                     char *result, size_t result_size)
{
  int macro[NKBDM];
  char expression[LISP_EXPRESSION_SIZE];
  const char *command_expression;
  const char *text;
  size_t argument_length;
  size_t expression_length;
  size_t length = 0;

  (void) result;
  (void) result_size;
  if (argc != 2 || argv[0][0] == 0 || strlen (argv[0]) >= NXNAME
      || argv[1][0] == 0
      || kbdmip != NULL || kbdmop != NULL)
    return FALSE;

  argument_length = strlen (argv[1]);
  if (argv[1][0] == '(' && argv[1][argument_length - 1] == ')')
    {
      command_expression = argv[1];
      expression_length = argument_length;
    }
  else
    {
      if (argument_length >= sizeof (expression) - 2)
        return FALSE;
      expression[0] = '(';
      memcpy (expression + 1, argv[1], argument_length);
      expression[argument_length + 1] = ')';
      expression[argument_length + 2] = 0;
      command_expression = expression;
      expression_length = argument_length + 2;
    }
  if (expression_length > NKBDM - LISP_COMMAND_OVERHEAD)
    return FALSE;

  macro[length++] = KMETA | 'X';
  for (text = "eval-lisp"; *text != 0; text++)
    macro[length++] = *text;
  macro[length++] = 0;
  for (text = command_expression; *text != 0; text++)
    macro[length++] = *text;
  macro[length++] = 0;
  macro[length++] = KCTLX | ')';

  return symdefmacro (argv[0], macro, length);
}

#ifdef COLOR
static int
lisp_set_syntax (int argc, const char *const *argv,
                 char *result, size_t result_size)
{
  (void) result;
  (void) result_size;
  if (argc != 1 || curbp == NULL)
    return FALSE;
  if (colorflag == FALSE)
    return TRUE;
  if (syntax_set_buffer (curbp, argv[0]) == FALSE)
    return FALSE;
  syntax_mode_changed (curbp);
  return TRUE;
}

static int
lisp_set_modeline_color (int argc, const char *const *argv,
                         char *result, size_t result_size)
{
  int colors[4];
  int index;

  (void) result;
  (void) result_size;
  if (argc != 4)
    return FALSE;
  for (index = 0; index < 4; index++)
    if (lisp_integer (argv[index], &colors[index]) == FALSE
        || colors[index] < 0 || colors[index] > 255)
      return FALSE;

  modefg256 = colors[0];
  modebg256 = colors[1];
  modefg = colors[2];
  modebg = colors[3];
  ttsetmodecolor (modefg256, modebg256, modefg, modebg);
  updatemode ();
  return TRUE;
}
#endif

static int
lisp_append_text (char *result, size_t *length, size_t result_size,
                  const char *text, size_t text_length)
{
  if (*length + text_length >= result_size)
    return FALSE;
  memcpy (result + *length, text, text_length);
  *length += text_length;
  return TRUE;
}

static int
lisp_format_message (int argc, const char *const *argv,
                     char *result, size_t result_size)
{
  size_t length = 0;
  size_t index;
  int argument;

  if (argc < 1)
    return FALSE;
  argument = 1;
  for (index = 0; argv[0][index] != 0; index++)
    {
      char character = argv[0][index];

      if (character != '%')
        {
          if (lisp_append_text (result, &length, result_size,
                                &character, 1) == FALSE)
            return FALSE;
          continue;
        }
      index++;
      character = argv[0][index];
      if (character == '%')
        {
          if (lisp_append_text (result, &length, result_size,
                                "%", 1) == FALSE)
            return FALSE;
        }
      else if (character == 's' || character == 'S')
        {
          if (argument >= argc
              || lisp_append_text (result, &length, result_size,
                                   argv[argument],
                                   strlen (argv[argument])) == FALSE)
            return FALSE;
          argument++;
        }
      else if (character == 'd')
        {
          char number[32];
          int value;
          int number_length;

          if (argument >= argc
              || lisp_integer (argv[argument], &value) == FALSE)
            return FALSE;
          number_length = snprintf (number, sizeof (number), "%d", value);
          if (number_length < 0
              || (size_t) number_length >= sizeof (number)
              || lisp_append_text (result, &length, result_size,
                                   number, (size_t) number_length)
                 == FALSE)
            return FALSE;
          argument++;
        }
      else
        return FALSE;
    }
  if (argument != argc || length >= result_size)
    return FALSE;
  result[length] = 0;
  return TRUE;
}

static int
lisp_message_form (int argc, const char *const *argv,
                   char *result, size_t result_size)
{
  if (lisp_format_message (argc, argv, result, result_size) != TRUE)
    return FALSE;
  eprintf ("%s", result);
  lisp_message_displayed = TRUE;
  return TINY_LISP_HOST_STRING;
}

static int
lisp_set_debug_level (int argc, const char *const *argv,
                      char *result, size_t result_size)
{
  int level;

  (void) result;
  (void) result_size;
  if (argc != 1 || lisp_integer (argv[0], &level) == FALSE || level < 0)
    return FALSE;
  lisp_debug_level = level;
  return TRUE;
}

static int
lisp_debug_message (int argc, const char *const *argv,
                    char *result, size_t result_size)
{
  int level;

  if (argc < 2 || lisp_integer (argv[0], &level) == FALSE || level < 1
      || lisp_format_message (argc - 1, argv + 1, result, result_size)
         != TRUE)
    return FALSE;
  if (level <= lisp_debug_level)
    {
      eprintf ("[Lisp debug: %s]", result);
      lisp_message_displayed = TRUE;
    }
  return TRUE;
}

static int
lisp_set_variable (int argc, const char *const *argv,
                   char *result, size_t result_size)
{
  typedef struct LispVariable
  {
    const char *name;
    int *value;
  } LispVariable;

  static const LispVariable variables[] = {
    { "tab-size", &tabsize },
    { "tab-width", &tabsize },
    { "fill-column", &fillcol }
  };
  char *end;
  long value;
  size_t index;

  (void) result;
  (void) result_size;
  if (argc != 2)
    return FALSE;
  value = strtol (argv[1], &end, 10);
  if (argv[1][0] == 0 || *end != 0 || value <= 0 || value > INT_MAX)
    return FALSE;

  value = (int) value;
  for (index = 0; index < sizeof (variables) / sizeof (variables[0]);
       index++)
    if (strcmp (argv[0], variables[index].name) == 0)
      {
        *variables[index].value = value;
        return TRUE;
      }
  return FALSE;
}

static int
lisp_register_hosts (void)
{
  typedef struct LispHost
  {
    const char *name;
    TinyLispHostFunction function;
  } LispHost;

  static const LispHost hosts[] = {
    { "define-key", lisp_bind_key },
    { "set-variable", lisp_set_variable },
    { "message", lisp_message_form },
    { "set-lisp-debug", lisp_set_debug_level },
    { "debug-message", lisp_debug_message },
    { "execute-command", lisp_execute_command },
    { "editor-cmd", lisp_execute_command },
    { "get-char", lisp_get_char },
    { "insert-char", lisp_insert_char },
    { "get-line", lisp_get_line },
    { "save-buffer", lisp_save_buffer },
    { "kill-emacs", lisp_kill_emacs },
    { "insert-string", lisp_insert_string },
    { "goto-bol", lisp_goto_bol },
    { "goto-eol", lisp_goto_eol },
    { "forward-line", lisp_forward_line },
    { "eobp", lisp_eobp },
    { "define-lisp-command", lisp_define_command },
#ifdef COLOR
    { "set-modeline-color", lisp_set_modeline_color },
    { "set-syntax", lisp_set_syntax }
#endif
  };
  static int registered = FALSE;
  size_t index;

  if (registered == TRUE)
    return TRUE;
  for (index = 0; index < sizeof (hosts) / sizeof (hosts[0]); index++)
    if (tiny_lisp_define_host (hosts[index].name,
                               hosts[index].function) != 1)
      return FALSE;
  registered = TRUE;
  return TRUE;
}

static int
lisp_has_suffix (const char *filename, const char *suffix)
{
  size_t filename_length = strlen (filename);
  size_t suffix_length = strlen (suffix);
  size_t offset;

  if (filename_length < suffix_length)
    return FALSE;
  filename += filename_length - suffix_length;
  for (offset = 0; offset < suffix_length; offset++)
    if (tolower ((unsigned char) filename[offset]) != suffix[offset])
      return FALSE;
  return TRUE;
}

int
lispfile (const char *filename)
{
  return filename != NULL
         && (lisp_has_suffix (filename, ".l")
             || lisp_has_suffix (filename, ".lisp")
             || lisp_has_suffix (filename, ".mel"));
}

static int
lisp_open_file (const char *filename, FILE **file)
{
  char expanded[NFILEN];

  if (filename == NULL)
    {
      *file = fopen (LISP_CONFIG_FILE, "rb");
      if (*file != NULL)
        return TRUE;
      if (snprintf (expanded, sizeof (expanded), "~/%s",
                    LISP_CONFIG_FILE) >= (int) sizeof (expanded))
        return FALSE;
    }
  else
    {
      if (snprintf (expanded, sizeof (expanded), "%s",
                    filename) >= (int) sizeof (expanded))
        return FALSE;
    }

  filename = fftilde (expanded);
  *file = fopen (filename, "rb");
  return *file != NULL;
}

static int
lisp_read_file (const char *filename, char **source)
{
  FILE *file;
  long length;

  if (lisp_open_file (filename, &file) != TRUE)
    return FALSE;
  if (fseek (file, 0, SEEK_END) != 0 || (length = ftell (file)) < 0)
    {
      fclose (file);
      return FALSE;
    }
  rewind (file);
  *source = malloc ((size_t) length + 1);
  if (*source == NULL
      || fread (*source, 1, (size_t) length, file) != (size_t) length)
    {
      free (*source);
      *source = NULL;
      fclose (file);
      return FALSE;
    }
  (*source)[length] = 0;
  fclose (file);
  return TRUE;
}

static int
lispload (const char *filename, int report)
{
  char result[LISP_OUTPUT_SIZE];
  char *source;
  int status;

  if (lisp_register_hosts () != TRUE)
    {
      eprintf ("[Lisp host commands unavailable]");
      return FALSE;
    }
  if (lisp_read_file (filename, &source) != TRUE)
    {
      if (filename != NULL)
        eprintf ("Unable to open Lisp file %s", filename);
      return filename == NULL;
    }

  lisp_message_displayed = FALSE;
  status = tiny_lisp_eval_source (source, result, sizeof (result));
  free (source);
  if (status != TRUE)
    eprintf ("[Lisp error: %s]", result);
  else if (report != FALSE && lisp_message_displayed == FALSE
           && (lisp_debug_level > 0 || strcmp (result, "t") != 0))
    eprintf ("[Lisp: %s]", result);
  return status;
}

int
lispstartup (const char *filename)
{
  return lispload (filename, FALSE);
}

int
lisprun (const char *filename)
{
  return lispload (filename, TRUE);
}

int
lispevalfile (int f, int n, int k)
{
  char filename[NFILEN];
  int status;

  (void) f;
  (void) n;
  (void) k;

  status = egetfname ("Read Lisp: ", filename, NFILEN);
  if (status != TRUE)
    return status;
  adjustcase (filename);
  return lisprun (filename);
}

int
lispeval (int f, int n, int k)
{
  char expr[LISP_EXPRESSION_SIZE];
  char source[LISP_EXPRESSION_SIZE + 64];
  char result[LISP_OUTPUT_SIZE];
  int prefix;
  int status;

  (void) k;

  if (ereadv ("Lisp: ", expr, sizeof (expr), EFNEW | EFCR) != TRUE)
    return FALSE;
  if (expr[0] == 0)
    return FALSE;
  if (lisp_register_hosts () != TRUE)
    {
      eprintf ("[Lisp host commands unavailable]");
      return FALSE;
    }

  prefix = f != FALSE ? n : 1;
  if (snprintf (source, sizeof (source),
                "(progn (setq prefix-arg %d) %s)", prefix, expr)
      >= (int) sizeof (source))
    return FALSE;

  lisp_message_displayed = FALSE;
  status = tiny_lisp_eval (source, result, sizeof (result));
  if (status != TRUE)
    eprintf ("[Lisp error: %s]", result);
  else if (lisp_message_displayed == FALSE
           && (lisp_debug_level > 0 || strcmp (result, "t") != 0))
    eprintf ("[Lisp: %s]", result);
  return status;
}

#endif
