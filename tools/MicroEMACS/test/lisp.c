#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../lisp.c"

int fillcol;
int tabsize;
int colorflag;
BUFFER *curbp;
EWINDOW *curwp;
int *kbdmip;
int *kbdmop;

static const char *lisp_input;
static char lisp_message[256];
static int bound_key;
static SYMBOL *bound_symbol;
static int command_flag;
static int command_count;
static int goto_bol_called;
static int goto_eol_called;
static int forward_line_flag;
static int forward_line_count;
static int save_buffer_called;
static int kill_emacs_called;
static int undo_start_count;
static int undo_end_count;
static int newline_count;
static char inserted_text[256];
static const char *defined_macro_name;
static int defined_macro_count;
static int defined_macro_called;
static int defined_macro_count_argument;
static int command_macro[NKBDM] = { KCTLX | ')' };
static SYMBOL command_symbol = { NULL, 0, "buffer-menu", NULL, NULL };
static SYMBOL macro_symbol = { NULL, 0, "test-macro", NULL, command_macro };
static BUFFER syntax_buffer;
static char syntax_filetype[32];
static BUFFER *syntax_mode_buffer;
static EWINDOW eobp_window;
static BUFFER eobp_buffer;
static LINE eobp_line;
static LINE eobp_end;
static LINE *primitive_line;
static LINE primitive_end;
static BUFFER primitive_buffer;
static EWINDOW primitive_window;

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

int
egetfname (const char *prompt, char *buffer, int buffer_size)
{
  return ereadv (prompt, buffer, buffer_size, EFNEW | EFCR | EFFILE);
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
test_command (int flag, int count, int key)
{
  (void) key;
  command_flag = flag;
  command_count = count;
  return TRUE;
}

int
gotobol (int flag, int count, int key)
{
  (void) flag;
  (void) count;
  (void) key;
  goto_bol_called++;
  return TRUE;
}

int
gotoeol (int flag, int count, int key)
{
  (void) flag;
  (void) count;
  (void) key;
  goto_eol_called++;
  return TRUE;
}

int
forwline (int flag, int count, int key)
{
  (void) key;
  forward_line_flag = flag;
  forward_line_count = count;
  return TRUE;
}

int
filesave (int flag, int count, int key)
{
  (void) flag;
  (void) count;
  (void) key;
  save_buffer_called++;
  return TRUE;
}

int
quit (int flag, int count, int key)
{
  (void) flag;
  (void) count;
  (void) key;
  kill_emacs_called++;
  return TRUE;
}

int
linsert (int count, int character, char *text)
{
  size_t length = strlen (inserted_text);
  uchar utf8[6];
  int utf8_length;

  if (count < 0
      || length + (size_t) count >= sizeof (inserted_text))
    return FALSE;
  if (text == NULL)
    {
      utf8_length = uputc (character, utf8);
      memcpy (inserted_text + length, utf8, (size_t) utf8_length);
      inserted_text[length + utf8_length] = 0;
      return TRUE;
    }
  memcpy (inserted_text + length, text, (size_t) count);
  inserted_text[length + count] = 0;
  return TRUE;
}

int
lnewline (void)
{
  size_t length = strlen (inserted_text);

  if (length + 1 >= sizeof (inserted_text))
    return FALSE;
  inserted_text[length] = '\n';
  inserted_text[length + 1] = 0;
  newline_count++;
  return TRUE;
}

int
domacro (int *macro, int count)
{
  (void) macro;
  defined_macro_called++;
  defined_macro_count_argument = count;
  return TRUE;
}

void
startsaveundo (void)
{
  undo_start_count++;
}

void
endsaveundo (void)
{
  undo_end_count++;
}

int
symdefmacro (const char *name, const int *macro, size_t count)
{
  if (name == NULL || macro == NULL || count == 0
      || macro[count - 1] != (KCTLX | ')'))
    return FALSE;
  if (strcmp (name, "buffer-menu") == 0)
    return FALSE;
  defined_macro_name = name;
  defined_macro_count = (int) count;
  memcpy (command_macro, macro, count * sizeof (*macro));
  return TRUE;
}

char *
fftilde (char *arg)
{
  return arg;
}

void
adjustcase (char *filename)
{
  (void) filename;
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

static int
evaluate_with_prefix (const char *expression, int flag, int count)
{
  lisp_message[0] = 0;
  lisp_input = expression;
  return lispeval (flag, count, 0);
}

static void
set_primitive_line (const char *text)
{
  size_t length = strlen (text);

  primitive_line = malloc (LINEHDR_SIZE + length);
  assert (primitive_line != NULL);
  primitive_line->l_size = (int) length;
  primitive_line->l_used = (int) length;
  memcpy (lgets (primitive_line), text, length);

  lforw (primitive_line) = &primitive_end;
  lback (primitive_line) = &primitive_end;
  lforw (&primitive_end) = primitive_line;
  lback (&primitive_end) = primitive_line;

  primitive_buffer.b_linep = &primitive_end;
  primitive_window.w_bufp = &primitive_buffer;
  primitive_window.w_dot.p = primitive_line;
  primitive_window.w_dot.o = 0;
  curbp = &primitive_buffer;
  curwp = &primitive_window;
}

int
main (void)
{
  command_symbol.s_funcp = test_command;
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
  assert (lispfile ("runtime.L") == TRUE);
  assert (lispfile ("runtime.Lisp") == TRUE);
  assert (lispfile ("runtime.MEL") == TRUE);
  assert (lispfile ("runtime.pro") == FALSE);
  assert (lispfile (NULL) == FALSE);

  bound_key = 0;
  bound_symbol = NULL;
  assert (evaluate ("(define-key \"C-x C-b\" \"buffer-menu\")") == TRUE);
  assert (bound_key == (KCTLX | KCTRL | 'B'));
  assert (bound_symbol == &command_symbol);
  assert (evaluate ("(define-key \"C-x C-b\" \"unknown\")") == FALSE);
  assert (evaluate ("(define-key \"C-x C-b\")") == FALSE);

  curwp = (EWINDOW *) &curwp;
  undo_start_count = 0;
  undo_end_count = 0;
  assert (evaluate ("(execute-command \"buffer-menu\")") == TRUE);
  assert (command_flag == FALSE && command_count == 1);
  assert (evaluate ("(execute-command \"buffer-menu\" 3)") == TRUE);
  assert (command_flag == TRUE && command_count == 3);
  assert (evaluate ("(execute-command \"unknown\")") == FALSE);
  assert (undo_start_count == 2 && undo_end_count == 2);
  command_flag = FALSE;
  command_count = 0;
  assert (evaluate ("(editor-cmd \"buffer-menu\" 3)") == TRUE);
  assert (command_flag == TRUE && command_count == 3);

  undo_start_count = 0;
  undo_end_count = 0;
  assert (evaluate ("(insert-string \"hello\")") == TRUE);
  assert (strcmp (inserted_text, "hello") == 0);
  assert (evaluate ("(insert-string \"!\" 3)") == TRUE);
  assert (strcmp (inserted_text, "hello!!!") == 0);
  assert (evaluate ("(insert-string \"x\" 0)") == FALSE);
  assert (undo_start_count == 2 && undo_end_count == 2);

  undo_start_count = 0;
  undo_end_count = 0;
  assert (evaluate ("(insert-string \"\\nnext\")") == TRUE);
  assert (strcmp (inserted_text, "hello!!!\nnext") == 0);
  assert (newline_count == 1);
  assert (undo_start_count == 1 && undo_end_count == 1);

  assert (evaluate ("(goto-bol)") == TRUE);
  assert (goto_bol_called == 1);
  assert (evaluate ("(goto-eol)") == TRUE);
  assert (goto_eol_called == 1);
  assert (evaluate ("(forward-line)") == TRUE);
  assert (forward_line_flag == FALSE && forward_line_count == 1);
  assert (evaluate ("(forward-line -2)") == TRUE);
  assert (forward_line_flag == TRUE && forward_line_count == -2);

  set_primitive_line ("héllo");
  assert (evaluate ("(get-char)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: \"h\"]") == 0);
  primitive_window.w_dot.o = 1;
  assert (evaluate ("(get-char)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: \"é\"]") == 0);
  assert (evaluate ("(get-line)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: \"héllo\"]") == 0);
  primitive_window.w_dot.o = wllength (primitive_line);
  assert (evaluate ("(get-char)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: nil]") == 0);

  inserted_text[0] = 0;
  undo_start_count = 0;
  undo_end_count = 0;
  assert (evaluate ("(insert-char \"X\")") == TRUE);
  assert (strcmp (inserted_text, "X") == 0);
  assert (evaluate ("(insert-char 65)") == TRUE);
  assert (strcmp (inserted_text, "XA") == 0);
  assert (evaluate ("(insert-char 233)") == TRUE);
  assert (strcmp (inserted_text, "XAé") == 0);
  assert (evaluate ("(insert-char \"xy\")") == FALSE);
  assert (evaluate ("(insert-char \"\\n\")") == FALSE);
  assert (evaluate ("(insert-char 0)") == FALSE);
  assert (evaluate ("(insert-char)") == FALSE);
  assert (strcmp (inserted_text, "XAé") == 0);
  assert (undo_start_count == 6 && undo_end_count == 6);

  assert (evaluate ("#'format-log-line") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: format-log-line]") == 0);
  assert (evaluate ("(eq #'format-log-line 'format-log-line)") == TRUE);

  assert (evaluate ("(define-lisp-command \"format-log-line\" "
                   "#'format-log-line)") == TRUE);
  assert (strcmp (defined_macro_name, "format-log-line") == 0);
  assert (defined_macro_count == 30);
  assert (command_macro[0] == (KMETA | 'X'));
  assert (command_macro[1] == 'e');
  assert (command_macro[10] == 0);
  assert (command_macro[11] == '(');
  assert (command_macro[28] == 0);
  assert (command_macro[29] == (KCTLX | ')'));
  assert (evaluate ("(define-lisp-command \"format-log-line\" "
                   "\"(format-log-line)\")") == TRUE);
  assert (defined_macro_count == 30);
  assert (evaluate ("(define-lisp-command \"buffer-menu\" \"(x)\")")
          == FALSE);

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

  assert (evaluate ("(defun optional-value (&optional arg)"
                   " (if arg arg 42))") == TRUE);
  assert (evaluate ("(optional-value)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 42]") == 0);
  assert (evaluate ("(optional-value 7)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 7]") == 0);

  assert (evaluate ("(let ((x 2) (y 3)) (* x y))") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 6]") == 0);
  assert (evaluate ("(let ((x 0))"
                   " (dotimes (i 3) (setq x (+ x 1))) x)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 3]") == 0);
  assert (evaluate ("(unless (= 1 1) 9)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: nil]") == 0);
  assert (evaluate ("(unless (= 1 2) 9)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 9]") == 0);
  assert (evaluate ("(fboundp 'car)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: t]") == 0);
  assert (evaluate ("(fboundp 'save-buffer)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: t]") == 0);
  assert (evaluate ("(fboundp 'kill-emacs)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: t]") == 0);
  assert (evaluate ("(save-buffer)") == TRUE);
  assert (save_buffer_called == 1);
  assert (evaluate ("(kill-emacs)") == TRUE);
  assert (kill_emacs_called == 1);

  assert (evaluate ("(defun prefix-value (&optional arg)"
                   " (interactive \"p\") arg)") == TRUE);
  assert (evaluate_with_prefix ("(prefix-value)", FALSE, 1) == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 1]") == 0);
  assert (evaluate_with_prefix ("(prefix-value)", TRUE, 3) == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 3]") == 0);
  assert (evaluate ("(prefix-value 7)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 7]") == 0);
  assert (evaluate ("(interactive \"x\")") == FALSE);
  assert (evaluate ("(defun required-nil (x)"
                   " (interactive \"p\") x)") == TRUE);
  assert (evaluate_with_prefix ("(required-nil nil)", TRUE, 3) == TRUE);
  assert (strcmp (lisp_message, "[Lisp: nil]") == 0);

  eobp_end.l_bp = &eobp_line;
  eobp_line.l_bp = &eobp_end;
  eobp_buffer.b_linep = &eobp_end;
  eobp_window.w_dot.p = &eobp_line;
  curbp = &eobp_buffer;
  curwp = &eobp_window;
  assert (evaluate ("(fboundp 'eobp)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: t]") == 0);
  assert (evaluate ("(eobp)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: t]") == 0);
  eobp_window.w_dot.p = &eobp_end;
  assert (evaluate ("(eobp)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: nil]") == 0);

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

  {
    FILE *file = fopen ("runtime.l", "wb");

    assert (file != NULL);
    assert (fputs ("(+ 1 7)", file) != EOF);
    assert (fclose (file) == 0);
    lisp_input = "runtime.l";
    lisp_message[0] = 0;
    assert (lispevalfile (FALSE, 1, 0) == TRUE);
    assert (strcmp (lisp_message, "[Lisp: 8]") == 0);
    assert (remove ("runtime.l") == 0);
  }

  puts ("lisp tests passed");
  return 0;
}
