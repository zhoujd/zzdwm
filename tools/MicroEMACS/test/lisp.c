#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../lisp.c"

int fillcol;
int tabsize;

static const char *lisp_input;
static char lisp_message[256];
static int bound_key;
static SYMBOL *bound_symbol;
static SYMBOL command_symbol = { NULL, 0, "buffer-menu", NULL, NULL };

int
ereadv (const char *prompt, char *buffer, int buffer_size, int flag, ...)
{
  size_t length;

  (void) prompt;
  (void) flag;
  length = strlen (lisp_input);
  if (length >= (size_t) buffer_size)
    return FALSE;
  memcpy (buffer, lisp_input, length + 1);
  return TRUE;
}

void
eprintf (const char *format, ...)
{
  va_list args;

  va_start (args, format);
  vsnprintf (lisp_message, sizeof (lisp_message), format, args);
  va_end (args);
}

SYMBOL *
symlookup (const char *name)
{
  if (strcmp (name, command_symbol.s_name) == 0)
    return &command_symbol;
  return NULL;
}

void
setbinding (int key, SYMBOL *symbol)
{
  bound_key = key;
  bound_symbol = symbol;
}

static int
evaluate (const char *expression)
{
  lisp_message[0] = 0;
  lisp_input = expression;
  return lispeval (FALSE, 1, 0);
}

int
main (void)
{
  tabsize = 4;
  assert (evaluate ("(set-variable \"tab-width\" \"8\")") == TRUE);
  assert (tabsize == 8);
  assert (evaluate ("(set-variable \"tab-size\" 12)") == TRUE);
  assert (tabsize == 12);
  assert (evaluate ("(set-variable \"fill-column\" \"72\")") == TRUE);
  assert (fillcol == 72);
  assert (evaluate ("(set-variable \"unknown\" \"8\")") == FALSE);
  assert (evaluate ("(set-variable \"tab-width\" \"0\")") == FALSE);
  assert (evaluate ("(set-variable \"tab-width\" \"8junk\")") == FALSE);
  assert (evaluate ("(set-variable \"tab-width\" \"8\" \"extra\")") == FALSE);

  bound_key = 0;
  bound_symbol = NULL;
  assert (evaluate ("(define-key \"C-x C-b\" \"buffer-menu\")") == TRUE);
  assert (bound_key == (KCTLX | KCTRL | 'B'));
  assert (bound_symbol == &command_symbol);
  assert (evaluate ("(define-key \"C-x C-b\" \"unknown\")") == FALSE);
  assert (evaluate ("(define-key \"C-x C-b\")") == FALSE);

  assert (evaluate ("(+ 1 2)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 3]") == 0);
  assert (evaluate ("(defun plus1 (x) (+ x 1))") == TRUE);
  assert (evaluate ("(plus1 2)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 3]") == 0);
  assert (evaluate ("(defmacro inc (x) (list '+ x 1))") == TRUE);
  assert (evaluate ("(inc 2)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 3]") == 0);
  assert (evaluate ("(defun countdown (n) (if (= n 0) 0 (countdown (- n 1))))")
          == TRUE);
  assert (evaluate ("(countdown 2000)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 0]") == 0);

  assert (evaluate ("(+ 1 2) extra") == FALSE);
  assert (evaluate ("(+ 1") == FALSE);
  assert (evaluate ("") == FALSE);

  puts ("lisp tests passed");
  return 0;
}
