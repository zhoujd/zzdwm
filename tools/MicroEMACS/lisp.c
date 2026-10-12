/*
 * Filename lisp.c
 */

#include "def.h"
#include "tinylisp.h"
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
lisp_bind_key (int argc, const char *const *argv)
{
  SYMBOL *symbol;
  int key;

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
lisp_execute_command (int argc, const char *const *argv)
{
  SYMBOL *symbol;
  int count = 1;
  int explicit_count;

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
lisp_insert_string (int argc, const char *const *argv)
{
  int count = 1;
  int index;

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
lisp_goto_bol (int argc, const char *const *argv)
{
  (void) argv;
  if (argc != 0 || curwp == NULL)
    return FALSE;
  return gotobol (FALSE, 1, KRANDOM);
}

static int
lisp_goto_eol (int argc, const char *const *argv)
{
  (void) argv;
  if (argc != 0 || curwp == NULL)
    return FALSE;
  return gotoeol (FALSE, 1, KRANDOM);
}

static int
lisp_forward_line (int argc, const char *const *argv)
{
  int count = 1;

  if (argc > 1 || curwp == NULL)
    return FALSE;
  if (argc == 1 && lisp_integer (argv[0], &count) == FALSE)
    return FALSE;
  return forwline (argc == 1, count, KRANDOM);
}

static int
lisp_define_command (int argc, const char *const *argv)
{
  int macro[NKBDM];
  char expression[LISP_EXPRESSION_SIZE];
  const char *command_expression;
  const char *text;
  size_t argument_length;
  size_t expression_length;
  size_t length = 0;

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
lisp_set_syntax (int argc, const char *const *argv)
{
  if (argc != 1 || curbp == NULL)
    return FALSE;
  if (colorflag == FALSE)
    return TRUE;
  if (syntax_set_buffer (curbp, argv[0]) == FALSE)
    return FALSE;
  syntax_mode_changed (curbp);
  return TRUE;
}
#endif

static int
lisp_set_variable (int argc, const char *const *argv)
{
  char *end;
  long value;

  if (argc != 2)
    return FALSE;
  value = strtol (argv[1], &end, 10);
  if (argv[1][0] == 0 || *end != 0 || value <= 0 || value > INT_MAX)
    return FALSE;

  value = (int) value;
  if (strcmp (argv[0], "tab-size") == 0
      || strcmp (argv[0], "tab-width") == 0)
    tabsize = value;
  else if (strcmp (argv[0], "fill-column") == 0)
    fillcol = value;
  else
    return FALSE;
  return TRUE;
}

static int
lisp_register_hosts (void)
{
  static int registered = FALSE;

  if (registered == TRUE)
    return TRUE;
  registered = tiny_lisp_define_host ("define-key", lisp_bind_key) == 1
               && tiny_lisp_define_host ("set-variable",
                                         lisp_set_variable) == 1;
  registered = registered != FALSE
               && tiny_lisp_define_host ("execute-command",
                                         lisp_execute_command) == 1;
  registered = registered != FALSE
               && tiny_lisp_define_host ("insert-string",
                                         lisp_insert_string) == 1;
  registered = registered != FALSE
               && tiny_lisp_define_host ("goto-bol", lisp_goto_bol) == 1;
  registered = registered != FALSE
               && tiny_lisp_define_host ("goto-eol", lisp_goto_eol) == 1;
  registered = registered != FALSE
               && tiny_lisp_define_host ("forward-line",
                                         lisp_forward_line) == 1;
  registered = registered != FALSE
               && tiny_lisp_define_host ("define-lisp-command",
                                         lisp_define_command) == 1;
#ifdef COLOR
  registered = registered != FALSE
               && tiny_lisp_define_host ("set-syntax", lisp_set_syntax) == 1;
#endif
  return registered;
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

int
lispstartup (const char *filename)
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

  status = tiny_lisp_eval_source (source, result, sizeof (result));
  free (source);
  if (status != TRUE)
    eprintf ("[Lisp error: %s]", result);
  return status;
}

int
lispeval (int f, int n, int k)
{
  char expr[LISP_EXPRESSION_SIZE];
  char result[LISP_OUTPUT_SIZE];
  int status;

  (void) f;
  (void) n;
  (void) k;

  if (ereadv ("Lisp: ", expr, sizeof (expr), EFNEW | EFCR) != TRUE)
    return FALSE;
  if (lisp_register_hosts () != TRUE)
    {
      eprintf ("[Lisp host commands unavailable]");
      return FALSE;
    }

  status = tiny_lisp_eval (expr, result, sizeof (result));
  if (status == TRUE)
    eprintf ("[Lisp: %s]", result);
  else
    eprintf ("[Lisp error: %s]", result);
  return status;
}

#endif
