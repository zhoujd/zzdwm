#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../lisp.c"

int fillcol;
int tabsize;
int colorflag;
BUFFER *curbp;

static const char *lisp_input;
static char lisp_message[256];
static int bound_key;
static SYMBOL *bound_symbol;
static SYMBOL command_symbol = { NULL, 0, "buffer-menu", NULL, NULL };
static BUFFER syntax_buffer;
static char syntax_filetype[32];
static BUFFER *syntax_mode_buffer;

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

char *
fftilde (char *arg)
{
  return arg;
}

int
syntax_set_buffer (BUFFER *bp, const char *filetype)
{
  if (colorflag == FALSE || bp != curbp || filetype == NULL
      || strcmp (filetype, "sql") != 0)
    return FALSE;
  snprintf (syntax_filetype, sizeof (syntax_filetype), "%s", filetype);
  return TRUE;
}

void
syntax_mode_changed (BUFFER *bp)
{
  syntax_mode_buffer = bp;
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

  colorflag = TRUE;
  curbp = &syntax_buffer;
  assert (evaluate ("(set-syntax \"sql\")") == TRUE);
  assert (strcmp (syntax_filetype, "sql") == 0);
  assert (syntax_mode_buffer == &syntax_buffer);
  colorflag = FALSE;
  assert (evaluate ("(set-syntax \"sql\")") == TRUE);

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

  {
    char result[LISP_OUTPUT_SIZE];
    int status;

    status = tiny_lisp_eval_source ("; MicroEMACS startup file\n"
                                    "(setq answer 42)\n"
                                    "answer", result, sizeof (result));
    if (status != 1)
      fprintf (stderr, "source error: %s\n", result);
    assert (status == 1);
    assert (strcmp (result, "42") == 0);
    assert (tiny_lisp_eval_source ("", result, sizeof (result)) == 1);
    assert (strcmp (result, "nil") == 0);
    assert (tiny_lisp_eval_source ("(+ 1 2) (bad)", result,
                                   sizeof (result)) == 0);
  }

  {
    FILE *file = fopen (".mel", "wb");

    assert (file != NULL);
    assert (fputs ("(set-variable \"tab-width\" \"9\")\n"
                   "(defun double (x) (* x 2))\n"
                   "(double 12)",
                   file) != EOF);
    assert (fclose (file) == 0);
    tabsize = 8;
    assert (lispstartup (NULL) == TRUE);
    assert (tabsize == 9);
    assert (remove (".mel") == 0);
    assert (lispstartup ("missing.lisp") == FALSE);
  }

  puts ("lisp tests passed");
  return 0;
}
