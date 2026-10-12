/*
 * Filename lisp.c
 */

#include "def.h"
#include "tinylisp.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LISP_EXPRESSION_SIZE 256
#define LISP_OUTPUT_SIZE     256
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
