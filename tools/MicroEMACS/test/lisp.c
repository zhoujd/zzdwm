#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../lisp.c"

int fillcol;
int tabsize;
int mouse_wheel_step;
int colorflag;
int modefg256 = 15;
int modebg256 = 22;
int modefg = 0;
int modebg = 2;
BUFFER *curbp;
EWINDOW *curwp;
int *kbdmip;
int *kbdmop;
int scriptmode;
int nrow = 30;
int ncol = 100;
int ttrow = 29;
int ttcol = 7;
int epresf;
int enoecho;
EWINDOW *wheadp;
BUFFER *bheadp;
static BUFFER feature_buffer;
BUFFER *blistp = &feature_buffer;

static const char *lisp_input;
static int lisp_input_status = TRUE;
static char lisp_message[256];
static int bound_key;
static SYMBOL *bound_symbol;
static int bound_mode_key;
static SYMBOL *bound_mode_symbol;
static int mode_created;
static char mode_name[32];
static char mode_storage;
static int bclear_called;
static BUFFER *bclear_buffer;
static char feature_lines[8][NCOL + 1];
static int feature_line_count;
static int gotobob_called;
static int bound_line_key;
static int bound_region_key;
static int command_flag;
static int command_count;
static int goto_bol_called;
static int goto_eol_called;
static char visited_file[NFILEN];
static int visit_file_status = TRUE;
static int forward_line_flag;
static int forward_line_count;
static int forward_line_status = TRUE;
static BUFFER switch_buffer;
static BUFFER dired_buffer;
static char switched_buffer_name[NBUFN];
static int save_buffer_called;
static int kill_emacs_called;
static int undo_start_count;
static int undo_end_count;
static int newline_count;
static char inserted_text[TINY_LISP_HOST_RESULT_SIZE];
static const char *defined_macro_name;
static int defined_macro_count;
static int defined_macro_called;
static int defined_macro_count_argument;
static int command_macro[NKBDM] = { KCTLX | ')' };
static SYMBOL command_symbol = { NULL, 0, "buffer-menu", NULL, NULL };
static SYMBOL macro_symbol = { NULL, 0, "test-macro", NULL, command_macro };
static SYMBOL ai_command_symbol = { NULL, 0, "ai-ask-line", NULL, NULL };
static SYMBOL ai_region_command_symbol = { NULL, 0, "ai-ask-region",
                                           NULL, NULL };
static SYMBOL lisp_dired_symbol = { NULL, 0, "lisp-dired", NULL, NULL };
static SYMBOL dired_refresh_symbol = { NULL, 0, "dired-refresh",
                                       NULL, NULL };
static SYMBOL dired_find_file_symbol = { NULL, 0, "dired-find-file",
                                         NULL, NULL };
static SYMBOL dired_up_directory_symbol = { NULL, 0, "dired-up-directory",
                                            NULL, NULL };
static SYMBOL dired_quit_symbol = { NULL, 0, "dired-quit", NULL, NULL };
static SYMBOL back_buffer_symbol = { NULL, 0, "back-buffer", NULL, NULL };
static SYMBOL smart_find_file_symbol = { NULL, 0, "smart-find-file",
                                         NULL, NULL };
static int ai_command_defined;
static int ai_region_command_defined;
static int lisp_dired_defined;
static int dired_refresh_defined;
static int dired_find_file_defined;
static int dired_up_directory_defined;
static int dired_quit_defined;
static int back_buffer_called;
static int smart_find_file_defined;
static BUFFER syntax_buffer;
static char syntax_filetype[32];
static BUFFER *syntax_mode_buffer;
static int tt_mode_color_called;
static int updatemode_called;
static const char *ai_prompt;
static char *ai_model;
static int ai_num_predict;
static int ai_status = TRUE;
static char ai_endpoint[256];
static char ai_api_key[256];
static char ai_reasoning_effort[32];
static int ai_timeout;
static int ai_configure_status = TRUE;
static char shell_command[NLINE];
static int shell_status = TRUE;
static char shell_output[NLINE];
static int getregion_status = TRUE;
static REGION test_region;
static LINE switch_end;
static EWINDOW eobp_window;
static BUFFER eobp_buffer;
static LINE eobp_line;
static LINE eobp_end;
static LINE *primitive_line;
static LINE primitive_end;
static LINE dired_end;
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
  return lisp_input_status;
}

int
egetfname (const char *prompt, char *buffer, int buffer_size)
{
  return ereadv (prompt, buffer, buffer_size, EFNEW | EFCR | EFFILE);
}

int
egetdname (const char *prompt, char *buffer, int buffer_size)
{
  return ereadv (prompt, buffer, buffer_size,
                 EFNEW | EFCR | EFFILE | EFDIR);
}

void
eprintf (const char *format, ...)
{
  va_list args;

  va_start (args, format);
  vsnprintf (lisp_message, sizeof (lisp_message), format, args);
  va_end (args);
}

void
eputs (const char *text)
{
  strncat (lisp_message, text,
           sizeof (lisp_message) - strlen (lisp_message) - 1);
}

void
ttmove (int row, int column)
{
  (void) row;
  (void) column;
}

void
tteeol (void)
{
}

void
ttflush (void)
{
}

void
ttsetmodecolor (int fg256, int bg256, int fg, int bg)
{
  tt_mode_color_called++;
  assert (fg256 == modefg256);
  assert (bg256 == modebg256);
  assert (fg == modefg);
  assert (bg == modebg);
}

void
updatemode (void)
{
  updatemode_called++;
}

SYMBOL *
symlookup (const char *name)
{
  if (ai_command_defined
      && strcmp (name, ai_command_symbol.s_name) == 0)
    return &ai_command_symbol;
  if (ai_region_command_defined
      && strcmp (name, ai_region_command_symbol.s_name) == 0)
    return &ai_region_command_symbol;
  if (lisp_dired_defined
      && strcmp (name, lisp_dired_symbol.s_name) == 0)
    return &lisp_dired_symbol;
  if (dired_refresh_defined
      && strcmp (name, dired_refresh_symbol.s_name) == 0)
    return &dired_refresh_symbol;
  if (dired_find_file_defined
      && strcmp (name, dired_find_file_symbol.s_name) == 0)
    return &dired_find_file_symbol;
  if (dired_up_directory_defined
      && strcmp (name, dired_up_directory_symbol.s_name) == 0)
    return &dired_up_directory_symbol;
  if (dired_quit_defined
      && strcmp (name, dired_quit_symbol.s_name) == 0)
    return &dired_quit_symbol;
  if (back_buffer_symbol.s_funcp != NULL
      && strcmp (name, back_buffer_symbol.s_name) == 0)
    return &back_buffer_symbol;
  if (strcmp (name, command_symbol.s_name) == 0)
    return &command_symbol;
  if (strcmp (name, smart_find_file_symbol.s_name) == 0)
    return &smart_find_file_symbol;
  return NULL;
}

void
setbinding (int key, SYMBOL *symbol)
{
  bound_key = key;
  bound_symbol = symbol;
  if (symbol == &ai_command_symbol)
    bound_line_key = key;
  if (symbol == &ai_region_command_symbol)
    bound_region_key = key;
}

void
setmodebinding (int key, SYMBOL *symbol)
{
  bound_mode_key = key;
  bound_mode_symbol = symbol;
}

void
createmode (const char *name)
{
  mode_created++;
  snprintf (mode_name, sizeof (mode_name), "%s", name);
  curbp->b_mode = (struct MODE *) &mode_storage;
}

int
bclear (BUFFER *buffer)
{
  bclear_called++;
  bclear_buffer = buffer;
  buffer->b_dot.p = buffer->b_linep;
  buffer->b_dot.o = 0;
  feature_line_count = 0;
  return TRUE;
}

int
addline (const char *text)
{
  if (feature_line_count >= (int) (sizeof (feature_lines)
                                   / sizeof (feature_lines[0])))
    return FALSE;
  snprintf (feature_lines[feature_line_count],
            sizeof (feature_lines[feature_line_count]), "%s", text);
  feature_line_count++;
  return TRUE;
}

int
popblist (void)
{
  return TRUE;
}

int
gotobob (int flag, int count, int key)
{
  (void) flag;
  (void) count;
  (void) key;
  curwp->w_dot.p = curbp->b_linep;
  curwp->w_dot.o = 0;
  gotobob_called++;
  return TRUE;
}

static int
test_command (int flag, int count, int key)
{
  (void) key;
  command_flag = flag;
  command_count = count;
  return TRUE;
}

static int
test_back_buffer (int flag, int count, int key)
{
  (void) flag;
  (void) count;
  (void) key;
  back_buffer_called++;
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
visit_file (char *filename)
{
  snprintf (visited_file, sizeof (visited_file), "%s", filename);
  return visit_file_status;
}

int
forwline (int flag, int count, int key)
{
  (void) key;
  forward_line_flag = flag;
  forward_line_count = count;
  return forward_line_status;
}

BUFFER *
bfind (const char *name, int create)
{
  (void) create;
  if (name != NULL && strcmp (name, "*dired*") == 0)
    return &dired_buffer;
  if (name == NULL || strcmp (name, "*AI*") != 0)
    return NULL;
  return &switch_buffer;
}

int
swbuffer (BUFFER *buffer)
{
  if (buffer != &switch_buffer && buffer != &dired_buffer)
    return FALSE;
  curbp = buffer;
  curwp->w_bufp = buffer;
  curwp->w_dot.p = buffer->b_linep = buffer == &dired_buffer
                    ? &dired_end : &switch_end;
  lforw (curwp->w_dot.p) = curwp->w_dot.p;
  lback (curwp->w_dot.p) = curwp->w_dot.p;
  snprintf (switched_buffer_name, sizeof (switched_buffer_name),
            buffer == &dired_buffer ? "*dired*" : "*AI*");
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
  if (strcmp (name, ai_command_symbol.s_name) == 0)
    {
      ai_command_defined = TRUE;
      ai_command_symbol.s_macro = command_macro;
    }
  if (strcmp (name, ai_region_command_symbol.s_name) == 0)
    {
      ai_region_command_defined = TRUE;
      ai_region_command_symbol.s_macro = command_macro;
    }
  if (strcmp (name, lisp_dired_symbol.s_name) == 0)
    lisp_dired_defined = TRUE;
  if (strcmp (name, dired_refresh_symbol.s_name) == 0)
    dired_refresh_defined = TRUE;
  if (strcmp (name, dired_find_file_symbol.s_name) == 0)
    dired_find_file_defined = TRUE;
  if (strcmp (name, dired_up_directory_symbol.s_name) == 0)
    dired_up_directory_defined = TRUE;
  if (strcmp (name, dired_quit_symbol.s_name) == 0)
    dired_quit_defined = TRUE;
  if (strcmp (name, smart_find_file_symbol.s_name) == 0)
    smart_find_file_defined = TRUE;
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
      || (strcmp (filetype, "text") != 0
          && strcmp (filetype, "sql") != 0))
    return FALSE;
  snprintf (syntax_filetype, sizeof (syntax_filetype), "%s", filetype);
  return TRUE;
}

void
syntax_mode_changed (BUFFER *bp)
{
  syntax_mode_buffer = bp;
}

int
ai_complete (const char *prompt, const char *model, int num_predict,
             char *output, size_t output_size)
{
  size_t index;
  char *prompt_copy;
  char *model_copy;

  prompt_copy = strdup (prompt);
  model_copy = model != NULL ? strdup (model) : NULL;
  free ((void *) ai_prompt);
  free ((void *) ai_model);
  ai_prompt = prompt_copy;
  ai_model = model_copy;
  ai_num_predict = num_predict;
  if (ai_status != TRUE)
    return FALSE;
  for (index = 0; index + 1 < output_size; index++)
    output[index] = 'x';
  if (index + 1 != output_size)
    return FALSE;
  output[index] = 0;
  return TRUE;
}

const char *
ai_last_error (void)
{
  return "mock error";
}

int
shell_capture (const char *command, char *output, int output_size)
{
  snprintf (shell_command, sizeof (shell_command), "%s", command);
  snprintf (output, (size_t) output_size, "%s", shell_output);
  return shell_status;
}

int
ai_configure (const char *endpoint, const char *api_key)
{
  snprintf (ai_endpoint, sizeof (ai_endpoint), "%s",
            endpoint != NULL ? endpoint : "");
  snprintf (ai_api_key, sizeof (ai_api_key), "%s",
            api_key != NULL ? api_key : "");
  return ai_configure_status;
}

int
ai_set_reasoning_effort (const char *effort)
{
  snprintf (ai_reasoning_effort, sizeof (ai_reasoning_effort), "%s",
            effort != NULL ? effort : "");
  return effort == NULL || effort[0] == 0
         || strcmp (effort, "minimal") == 0
         || strcmp (effort, "low") == 0
         || strcmp (effort, "medium") == 0
         || strcmp (effort, "high") == 0;
}

int
ai_set_timeout (int seconds)
{
  ai_timeout = seconds;
  return seconds > 0 && seconds <= 86400;
}

int
getregion (REGION *region)
{
  if (getregion_status != TRUE)
    {
      eprintf ("No mark in this window");
      return FALSE;
    }
  *region = test_region;
  return TRUE;
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

static int
evaluate_source (const char *source)
{
  char *copy = strdup (source);

  assert (copy != NULL);
  lisp_message[0] = 0;
  return lisp_evaluate_source (copy, TRUE);
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

static int
evaluate_example_file (const char *filename, char *result, size_t result_size)
{
  char path[256];
  char *source;
  FILE *file;
  long length;
  int status;

  if (snprintf (path, sizeof (path), "%s", filename)
      >= (int) sizeof (path))
    return FALSE;
  file = fopen (path, "rb");
  if (file == NULL)
    {
      if (snprintf (path, sizeof (path), "../%s", filename)
          >= (int) sizeof (path))
        return FALSE;
      file = fopen (path, "rb");
    }
  if (file == NULL
      || fseek (file, 0, SEEK_END) != 0
      || (length = ftell (file)) < 0)
    {
      if (file != NULL)
        fclose (file);
      return FALSE;
    }
  rewind (file);
  source = malloc ((size_t) length + 1);
  if (source == NULL
      || fread (source, 1, (size_t) length, file) != (size_t) length)
    {
      free (source);
      fclose (file);
      return FALSE;
    }
  source[length] = 0;
  fclose (file);
  status = tiny_lisp_eval_source (source, result, result_size);
  free (source);
  return status;
}

int
main (void)
{
  command_symbol.s_funcp = test_command;
  back_buffer_symbol.s_funcp = test_back_buffer;
  memset (&feature_buffer, 0, sizeof (feature_buffer));
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

  {
    char result[LISP_OUTPUT_SIZE];

    shell_status = FALSE;
    shell_output[0] = 0;
    assert (tiny_lisp_eval ("(shell-command-to-string \"uname -r\")",
                            result, sizeof (result)) == 1);
    assert (strcmp (shell_command, "uname -r") == 0);
    assert (strcmp (result, "nil") == 0);
    assert (strcmp (lisp_message, "[Lisp: shell command output failed]")
            == 0);
    shell_status = TRUE;
    strcpy (shell_output, "6.1.0\n");
    assert (tiny_lisp_eval ("(shell-command-to-string \"uname -r\")",
                            result, sizeof (result)) == 1);
    assert (strcmp (result, "\"6.1.0\\n\"") == 0);
    assert (tiny_lisp_eval ("(shell-command-to-string)",
                            result, sizeof (result)) == 0);
    assert (tiny_lisp_eval ("(shell-command-to-string \"\" \"extra\")",
                            result, sizeof (result)) == 0);
    shell_status = FALSE;
  }

  {
    char result[LISP_OUTPUT_SIZE];

    lisp_message[0] = 0;
    assert (tiny_lisp_eval ("(message \"hello %s\" \"world\")",
                            result, sizeof (result)) == 1);
    assert (strcmp (lisp_message, "hello world") == 0);
    assert (strcmp (result, "\"hello world\"") == 0);
    assert (tiny_lisp_eval ("(message \"count %d%%\" 3)",
                            result, sizeof (result)) == 1);
    assert (strcmp (lisp_message, "count 3%") == 0);
    assert (strcmp (result, "\"count 3%\"") == 0);
    assert (tiny_lisp_eval ("(message)", result, sizeof (result)) == 0);
    assert (tiny_lisp_eval ("(message \"unused\" \"argument\")",
                            result, sizeof (result)) == 0);
    assert (tiny_lisp_eval ("(message \"bad %q\" \"argument\")",
                            result, sizeof (result)) == 0);
    assert (evaluate ("(message \"interactive %s\" \"test\")") == TRUE);
    assert (strcmp (lisp_message, "interactive test") == 0);
  }

  assert (evaluate ("(set-lisp-debug -1)") == FALSE);
  assert (evaluate ("(set-lisp-debug 2)") == TRUE);
  assert (lisp_debug_level == 2);
  lisp_message[0] = 0;
  assert (evaluate ("(debug-message 1 \"step %s %d\" \"one\" 2)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp debug: step one 2]") == 0);
  assert (evaluate ("(debug-message 3 \"hidden\")") == TRUE);
  assert (strncmp (lisp_message, "[Lisp debug:", 12) != 0);
  assert (evaluate ("(debug-message 0 \"invalid-level\")") == FALSE);
  assert (evaluate ("(debug-message 1 \"bad %q\" \"argument\")") == FALSE);
  assert (evaluate ("(set-lisp-debug 0)") == TRUE);
  assert (lisp_debug_level == 0);
  lisp_message[0] = 0;
  assert (evaluate ("(setq default-quiet-result t)") == TRUE);
  assert (lisp_message[0] == 0);
  assert (evaluate ("(set-lisp-debug 1)") == TRUE);
  lisp_message[0] = 0;
  assert (evaluate ("(setq debug-quiet-result t)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: t]") == 0);
  assert (evaluate ("(set-lisp-debug 0)") == TRUE);
  assert (lisp_debug_level == 0);

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
  forward_line_status = FALSE;
  assert (evaluate ("(forward-line -1)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: nil]") == 0);
  forward_line_status = TRUE;

  set_primitive_line ("héllo");
  assert (evaluate ("(get-char)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: \"h\"]") == 0);
  primitive_window.w_dot.o = 1;
  assert (evaluate ("(get-char)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: \"é\"]") == 0);
  assert (evaluate ("(get-line)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: \"héllo\"]") == 0);
  assert (evaluate ("(get-line 0)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: \"héllo\"]") == 0);
  assert (evaluate ("(get-line 1)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: nil]") == 0);
  assert (evaluate ("(get-line -1)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: nil]") == 0);
  primitive_window.w_dot.o = wllength (primitive_line);
  assert (evaluate ("(get-char)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: nil]") == 0);
  test_region.r_pos.p = primitive_line;
  test_region.r_pos.o = 0;
  test_region.r_size = wllength (primitive_line) + 1;
  inserted_text[0] = 0;
  assert (evaluate ("(insert-string (get-region))") == TRUE);
  assert (strcmp (inserted_text, "h\xc3\xa9llo\n") == 0);
  assert (evaluate ("(get-region 1)") == FALSE);
  test_region.r_size = (long) TINY_LISP_HOST_RESULT_SIZE + 1;
  lisp_message[0] = 0;
  assert (evaluate ("(get-region)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: region is too large]") == 0);
  test_region.r_size = wllength (primitive_line) + 1;
  getregion_status = FALSE;
  lisp_message[0] = 0;
  assert (evaluate ("(get-region)") == TRUE);
  assert (strcmp (lisp_message, "No mark in this window") == 0);
  getregion_status = TRUE;

  {
    char result[LISP_OUTPUT_SIZE];
    const char *directory = "/tmp/microemacs-dired-test";
    FILE *file;

    assert (tiny_lisp_eval ("(string-trim-suffix \"directory/\" \"/\")",
                            result, sizeof (result)) == 1);
    assert (strcmp (result, "\"directory\"") == 0);
    assert (tiny_lisp_eval ("(join-path \"/tmp\" \"file.c\")",
                            result, sizeof (result)) == 1);
    assert (strcmp (result, "\"/tmp/file.c\"") == 0);
    assert (tiny_lisp_eval ("(join-path \"C:/dir/\" \"file.c\")",
                            result, sizeof (result)) == 1);
    assert (strcmp (result, "\"C:/dir/file.c\"") == 0);
    assert (tiny_lisp_eval ("(resolve-path \"/nonexistent-me-path\")",
                            result, sizeof (result)) == 1);
    assert (strcmp (result, "nil") == 0);
    {
      FILE *file = fopen ("/tmp/microemacs-resolve-target", "wb");

      assert (file != NULL);
      assert (fclose (file) == 0);
      assert (remove ("/tmp/microemacs-resolve-link") == 0
              || errno == ENOENT);
      assert (symlink ("/tmp/microemacs-resolve-target",
                       "/tmp/microemacs-resolve-link") == 0);
      assert (tiny_lisp_eval ("(resolve-path \"/tmp/microemacs-resolve-link\")",
                              result, sizeof (result)) == 1);
      assert (strcmp (result, "\"/tmp/microemacs-resolve-target\"") == 0);
      assert (remove ("/tmp/microemacs-resolve-link") == 0);
      assert (remove ("/tmp/microemacs-resolve-target") == 0);
    }
    assert (evaluate ("(resolve-path \"\")") == FALSE);
    assert (evaluate ("(resolve-path \"one\" \"two\")") == FALSE);
    scriptmode = TRUE;
    assert (tiny_lisp_eval ("(read-directory-name \"Dired\" \"default\")",
                            result, sizeof (result)) == 1);
    assert (strcmp (result, "\"default\"") == 0);
    scriptmode = FALSE;
    lisp_input = "/selected";
    assert (tiny_lisp_eval ("(read-directory-name \"Dired\" \".\")",
                            result, sizeof (result)) == 1);
    assert (strcmp (result, "\"/selected\"") == 0);
    lisp_input = "";
    assert (tiny_lisp_eval ("(read-directory-name \"Dired\" \".\")",
                            result, sizeof (result)) == 1);
    assert (strcmp (result, "\".\"") == 0);
    scriptmode = TRUE;
    assert (tiny_lisp_eval ("(read-file-name \"Find file\" \"default.c\")",
                            result, sizeof (result)) == 1);
    assert (strcmp (result, "\"default.c\"") == 0);
    scriptmode = FALSE;
    assert (tiny_lisp_eval ("(read-file-name \"Find file\")",
                            result, sizeof (result)) == 1);
    assert (strcmp (result, "nil") == 0);
    lisp_input_status = ABORT;
    assert (evaluate_source ("(read-directory-name \"Dired\" \".\")")
            == TRUE);
    assert (strcmp (lisp_message, "^G") == 0);
    assert (evaluate_source ("(read-file-name \"Find file\")") == TRUE);
    assert (strcmp (lisp_message, "^G") == 0);
    lisp_input_status = TRUE;
    assert (evaluate ("(read-file-name)") == FALSE);
    assert (evaluate ("(read-file-name \"one\" \"two\" \"three\")")
            == FALSE);
    assert (evaluate ("(string-suffix-p \"directory/\" \"/\")") == TRUE);
    assert (lisp_message[0] == 0);
    assert (evaluate ("(string-suffix-p \"file.c\" \"/\")") == TRUE);
    assert (strcmp (lisp_message, "[Lisp: nil]") == 0);

    strcpy (primitive_buffer.b_fname, "/current/file.c");
    assert (tiny_lisp_eval ("(buffer-file-name)",
                            result, sizeof (result)) == 1);
    assert (strcmp (result, "\"/current/file.c\"") == 0);

    assert (lisp_key_code ("h") == 'h');
    assert (lisp_key_code ("H") == 'H');
    assert (lisp_key_code ("C-h") == (KCTRL | 'H'));
    assert (evaluate ("(define-mode-key \"C-m\" \"buffer-menu\")")
            == FALSE);
    assert (evaluate ("(create-mode \"Dired\")") == TRUE);
    assert (mode_created == 1 && strcmp (mode_name, "Dired") == 0);
    assert (evaluate ("(define-mode-key \"C-m\" \"buffer-menu\")")
            == TRUE);
    assert (bound_mode_key == (KCTRL | 'M'));
    assert (bound_mode_symbol == &command_symbol);
    assert (evaluate ("(define-mode-key \"h\" \"buffer-menu\")")
            == TRUE);
    assert (bound_mode_key == 'h');

    remove ("/tmp/microemacs-dired-test/alpha.txt");
    rmdir ("/tmp/microemacs-dired-test/sub");
    rmdir (directory);
    assert (mkdir (directory, 0755) == 0);
    file = fopen ("/tmp/microemacs-dired-test/alpha.txt", "wb");
    assert (file != NULL);
    assert (fclose (file) == 0);
    assert (mkdir ("/tmp/microemacs-dired-test/sub", 0755) == 0);
    inserted_text[0] = 0;
    bclear_called = 0;
    bclear_buffer = NULL;
    gotobob_called = 0;
    assert (evaluate ("(dired-list \"/tmp/microemacs-dired-test\")")
            == TRUE);
    assert (bclear_called == 1 && bclear_buffer == &primitive_buffer);
    assert (gotobob_called == 2);
    assert (strcmp (primitive_buffer.b_fname, directory) == 0);
    assert (strncmp (inserted_text, "/tmp/microemacs-dired-test:\n",
                     strlen ("/tmp/microemacs-dired-test:\n")) == 0);
    assert (strstr (inserted_text, " ./\n") != NULL);
    assert (strstr (inserted_text, " ../\n") != NULL);
    assert (strstr (inserted_text, " sub/\n") != NULL);
    assert (strstr (inserted_text, " alpha.txt\n") != NULL);
    inserted_text[0] = 0;
    assert (evaluate
            ("(dired-list \"/tmp/microemacs-dired-test/../microemacs-dired-test\")")
            == TRUE);
    assert (strcmp (primitive_buffer.b_fname, directory) == 0);
    assert (strncmp (inserted_text, "/tmp/microemacs-dired-test:\n",
                     strlen ("/tmp/microemacs-dired-test:\n")) == 0);
    assert ((primitive_buffer.b_flag & BFRO) == 0);
    assert (evaluate ("(set-buffer-readonly t)") == TRUE);
    assert ((primitive_buffer.b_flag & BFRO) != 0);
    assert (evaluate ("(set-buffer-readonly nil)") == TRUE);
    assert ((primitive_buffer.b_flag & BFRO) == 0);

    {
      char entry[128];

      snprintf (entry, sizeof (entry),
                "%-10s %3llu %8lld %16s  %s",
                "drwxr-xr-x", 2ULL, 4096LL,
                "2026-10-15 10:00", "sub/");
      set_primitive_line (entry);
      assert (tiny_lisp_eval ("(dired-entry-name)",
                              result, sizeof (result)) == 1);
      assert (strcmp (result, "\"sub/\"") == 0);
      set_primitive_line ("/tmp/microemacs-dired-test:");
      assert (tiny_lisp_eval ("(dired-entry-name)",
                              result, sizeof (result)) == 1);
      assert (strcmp (result, "nil") == 0);
    }

    assert (remove ("/tmp/microemacs-dired-test/alpha.txt") == 0);
    assert (rmdir ("/tmp/microemacs-dired-test/sub") == 0);
    assert (rmdir (directory) == 0);
  }

  switched_buffer_name[0] = 0;
  assert (evaluate ("(switch-to-buffer \"*AI*\")") == TRUE);
  assert (strcmp (switched_buffer_name, "*AI*") == 0);
  assert (evaluate ("(switch-to-buffer)") == FALSE);
  assert (evaluate ("(switch-to-buffer \"\")") == FALSE);
  assert (setenv ("MICROEMACS_LISP_TEST_VALUE", "secret", 1) == 0);
  assert (evaluate ("(getenv \"MICROEMACS_LISP_TEST_VALUE\")") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: \"secret\"]") == 0);
  assert (unsetenv ("MICROEMACS_LISP_TEST_VALUE") == 0);
  assert (evaluate ("(getenv \"MICROEMACS_LISP_TEST_VALUE\")") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: nil]") == 0);
  assert (evaluate ("(getenv)") == FALSE);
  assert (evaluate ("(getenv \"\")") == FALSE);

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
  assert (evaluate ("(me-find-file \"src/main.c\")") == TRUE);
  assert (strcmp (visited_file, "src/main.c") == 0);
  visit_file_status = FALSE;
  assert (evaluate ("(me-find-file \"missing.c\")") == FALSE);
  visit_file_status = TRUE;
  assert (evaluate ("(me-find-file)") == FALSE);
  {
    char result[LISP_OUTPUT_SIZE];

    assert (tiny_lisp_eval ("#!/usr/bin/env me --script\n(+ 1 2)",
                            result, sizeof (result)) == 1);
    assert (strcmp (result, "3") == 0);
  }

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
  tt_mode_color_called = 0;
  updatemode_called = 0;
  assert (evaluate ("(set-syntax \"sql\")") == TRUE);
  assert (strcmp (syntax_filetype, "sql") == 0);
  assert (syntax_mode_buffer == &syntax_buffer);
  colorflag = FALSE;
  assert (evaluate ("(set-syntax \"sql\")") == TRUE);
  colorflag = TRUE;
  assert (evaluate ("(set-modeline-color 15 22 0 2)") == TRUE);
  assert (modefg256 == 15 && modebg256 == 22);
  assert (modefg == 0 && modebg == 2);
  assert (tt_mode_color_called == 1 && updatemode_called == 1);
  colorflag = FALSE;
  assert (evaluate ("(set-modeline-color 16 23 1 3)") == TRUE);
  assert (modefg256 == 16 && modebg256 == 23);
  assert (modefg == 1 && modebg == 3);
  assert (tt_mode_color_called == 2 && updatemode_called == 2);
  colorflag = TRUE;
  assert (evaluate ("(set-modeline-color 15 22 0)") == FALSE);
  assert (evaluate ("(set-modeline-color 15 22 0 256)") == FALSE);
  assert (evaluate ("(set-modeline-color 15 22 0 -1)") == FALSE);

  assert (evaluate ("(+ 1 2)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 3]") == 0);
  assert (evaluate ("(+ .1 .2)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 0.3]") == 0);
  assert (evaluate ("(- 5 3.2 .6)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 1.2]") == 0);
  assert (evaluate ("(/ 1 4)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 0.25]") == 0);
  assert (evaluate ("(< 2 4 6.8)") == TRUE);
  assert (lisp_message[0] == 0);
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
  assert (evaluate ("(count-char \"a{b}{\" \"{\")") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 2]") == 0);
  assert (evaluate ("(+ (count-char \"{}{\" \"{\") 1)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 3]") == 0);
  assert (evaluate ("(count-char \"x\" \"xy\")") == FALSE);
  assert (evaluate ("(count-char 1 \"x\")") == FALSE);
  assert (evaluate ("(let ((x 0))"
                   " (dotimes (i 3) (setq x (+ x 1))) x)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 3]") == 0);
  assert (evaluate ("(unless (= 1 1) 9)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: nil]") == 0);
  assert (evaluate ("(unless (= 1 2) 9)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 9]") == 0);
  assert (evaluate ("(fboundp 'car)") == TRUE);
  assert (lisp_message[0] == 0);
  assert (evaluate ("(fboundp 'save-buffer)") == TRUE);
  assert (lisp_message[0] == 0);
  assert (evaluate ("(fboundp 'kill-emacs)") == TRUE);
  assert (lisp_message[0] == 0);
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
  assert (evaluate ("(defun plain-interactive (&optional arg)"
                   " (interactive) (if arg arg 42))") == TRUE);
  assert (evaluate_with_prefix ("(plain-interactive)", FALSE, 1) == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 42]") == 0);
  assert (evaluate_with_prefix ("(plain-interactive)", TRUE, 3) == TRUE);
  assert (strcmp (lisp_message, "[Lisp: 42]") == 0);
  assert (evaluate ("(plain-interactive 7)") == TRUE);
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
  assert (lisp_message[0] == 0);
  assert (evaluate ("(eobp)") == TRUE);
  assert (lisp_message[0] == 0);
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
    FILE *file = fopen ("nested-load.l", "wb");

    assert (file != NULL);
    assert (fputs ("(defun nested-build-list (count list)\n"
                   "  (if (= count 0)\n"
                   "      list\n"
                   "    (nested-build-list (- count 1) (cons count list))))\n"
                   "(nested-build-list 300 nil)\n"
                   "(setq nested-load-value 42)\n"
                   "nested-load-value",
                   file) != EOF);
    assert (fclose (file) == 0);
    assert (evaluate ("(load \"nested-load.l\")") == TRUE);
    assert (evaluate ("nested-load-value") == TRUE);
    assert (strcmp (lisp_message, "[Lisp: 42]") == 0);
    assert (remove ("nested-load.l") == 0);
  }

  {
    FILE *file = fopen ("require-once.l", "wb");

    assert (file != NULL);
    assert (fputs ("(setq require-count 1)",
                   file) != EOF);
    assert (fclose (file) == 0);
    assert (evaluate ("(require \"require-once\")") == TRUE);
    assert (evaluate ("(require 'require-once)") == TRUE);
    assert (evaluate ("require-count") == TRUE);
    assert (strcmp (lisp_message, "[Lisp: 1]") == 0);
    assert (remove ("require-once.l") == 0);
  }

  {
    FILE *file = fopen ("provide-and-load.l", "wb");

    assert (file != NULL);
    assert (fputs ("(setq provide-count 1)\n"
                   "(provide 'provide-and-load)",
                   file) != EOF);
    assert (fclose (file) == 0);
    assert (evaluate ("(provide 'standalone-feature)") == TRUE);
    assert (evaluate ("features") == TRUE);
    assert (strcmp (lisp_message, "[Lisp: (standalone-feature)]") == 0);
    assert (evaluate ("(require 'standalone-feature)") == TRUE);
    assert (evaluate ("(provide-and-load-loaded)") == FALSE);
    assert (evaluate ("(require 'provide-and-load)") == TRUE);
    assert (evaluate ("provide-count") == TRUE);
    assert (strcmp (lisp_message, "[Lisp: 1]") == 0);
    assert (evaluate ("(require 'provide-and-load)") == TRUE);
    assert (evaluate ("(provide \"./invalid\")") == FALSE);
    assert (evaluate ("(provide 'valid \"extra\")") == FALSE);
    assert (remove ("provide-and-load.l") == 0);

    bclear_called = 0;
    bclear_buffer = NULL;
    ncol = 40;
    assert (lisplistfeatures (FALSE, 1, 0) == TRUE);
    ncol = 100;
    assert (bclear_called == 1 && bclear_buffer == blistp);
    assert (feature_line_count == 5);
    assert (strcmp (feature_lines[0], "Feature") == 0);
    assert (strcmp (feature_lines[1], "-------") == 0);
    assert (strcmp (feature_lines[2], "require-once    ") == 0);
    assert (strcmp (feature_lines[3], "standalone-feature              ") == 0);
    assert (strcmp (feature_lines[4], "provide-and-load                ") == 0);
  }

  assert (evaluate ("(require \"missing-lisp-module\" nil t)") == TRUE);
  assert (evaluate ("(require \"missing-lisp-module\")") == FALSE);

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

  {
    FILE *file = fopen ("runtime-true.l", "wb");

    assert (file != NULL);
    assert (fputs ("(setq loaded-configuration t)", file) != EOF);
    assert (fclose (file) == 0);
    lisp_input = "runtime-true.l";
    lisp_message[0] = 0;
    assert (lispevalfile (FALSE, 1, 0) == TRUE);
    assert (lisp_message[0] == 0);
    assert (remove ("runtime-true.l") == 0);
  }

  {
    FILE *file = fopen ("runtime-debug-true.l", "wb");

    assert (file != NULL);
    assert (fputs ("(set-lisp-debug 1)\n"
                   "(setq loaded-debug-configuration t)", file) != EOF);
    assert (fclose (file) == 0);
    lisp_input = "runtime-debug-true.l";
    lisp_message[0] = 0;
    assert (lispevalfile (FALSE, 1, 0) == TRUE);
    assert (strcmp (lisp_message, "[Lisp: t]") == 0);
    assert (remove ("runtime-debug-true.l") == 0);
    assert (evaluate ("(set-lisp-debug 0)") == TRUE);
  }

  {
    FILE *file = fopen ("runtime-suppressed-debug.l", "wb");

    assert (file != NULL);
    assert (fputs ("(debug-message 2 \"hidden startup detail\")", file)
            != EOF);
    assert (fclose (file) == 0);
    lisp_input = "runtime-suppressed-debug.l";
    lisp_message[0] = 0;
    assert (lispevalfile (FALSE, 1, 0) == TRUE);
    assert (lisp_message[0] == 0);
    assert (remove ("runtime-suppressed-debug.l") == 0);
  }

  ai_prompt = NULL;
  ai_model = NULL;
  ai_num_predict = 0;
  assert (evaluate ("(ai-complete \"prompt\" \"model\" 3)") == TRUE);
  assert (strcmp (ai_prompt, "prompt") == 0);
  assert (strcmp (ai_model, "model") == 0);
  assert (ai_num_predict == 3);
  ai_status = FALSE;
  assert (evaluate ("(ai-complete \"prompt\")") == TRUE);
  assert (strcmp (lisp_message, "[AI: mock error]") == 0);
  assert (evaluate ("(ai-last-error)") == TRUE);
  assert (strcmp (lisp_message, "[Lisp: \"mock error\"]") == 0);
  assert (ai_model == NULL && ai_num_predict == 0);
  ai_status = TRUE;
  assert (evaluate ("(ai-complete)") == FALSE);
  assert (evaluate ("(ai-complete \"x\" \"model\" 0)") == FALSE);
  inserted_text[0] = 0;
  undo_start_count = 0;
  undo_end_count = 0;
  assert (evaluate ("(insert-string (ai-complete \"long prompt\"))") == TRUE);
  assert (strlen (inserted_text) == TINY_LISP_HOST_RESULT_SIZE - 1);
  assert (undo_start_count == 1 && undo_end_count == 1);
  ai_endpoint[0] = 0;
  ai_api_key[0] = 0;
  assert (evaluate ("(ai-set-endpoint \"http://127.0.0.1:11434/api/generate\")")
          == TRUE);
  assert (strcmp (ai_endpoint, "http://127.0.0.1:11434/api/generate") == 0);
  assert (ai_api_key[0] == 0);
  assert (evaluate ("(ai-set-endpoint \"http://example.test/v1/chat/completions\" "
                    "\"test-key\")") == TRUE);
  assert (strcmp (ai_endpoint, "http://example.test/v1/chat/completions") == 0);
  assert (strcmp (ai_api_key, "test-key") == 0);
  ai_configure_status = FALSE;
  assert (evaluate ("(ai-set-endpoint \"bad-endpoint\")") == TRUE);
  assert (strcmp (lisp_message, "[AI: mock error]") == 0);
  ai_configure_status = TRUE;
  assert (evaluate ("(ai-set-endpoint)") == FALSE);
  assert (evaluate ("(ai-set-endpoint \"http://example.test\" \"\")")
          == FALSE);
  ai_reasoning_effort[0] = 0;
  assert (evaluate ("(ai-set-reasoning-effort \"low\")") == TRUE);
  assert (strcmp (ai_reasoning_effort, "low") == 0);
  assert (evaluate ("(ai-set-reasoning-effort \"\")") == TRUE);
  assert (ai_reasoning_effort[0] == 0);
  assert (evaluate ("(ai-set-reasoning-effort \"invalid\")") == TRUE);
  assert (strcmp (lisp_message, "[AI: mock error]") == 0);
  assert (evaluate ("(ai-set-reasoning-effort)") == FALSE);
  ai_timeout = 0;
  assert (evaluate ("(ai-set-timeout 180)") == TRUE);
  assert (ai_timeout == 180);
  assert (evaluate ("(ai-set-timeout 0)") == TRUE);
  assert (strcmp (lisp_message, "[AI: mock error]") == 0);
  assert (evaluate ("(ai-set-timeout)") == FALSE);

  set_primitive_line ("héllo");
  test_region.r_pos.p = primitive_line;
  test_region.r_pos.o = 0;
  test_region.r_size = 6;
  getregion_status = TRUE;
  ai_prompt = NULL;
  ai_model = NULL;
  ai_num_predict = 0;
  assert (evaluate ("(ai-complete-region)") == TRUE);
  assert (strcmp (ai_prompt, "h\xc3\xa9llo\n") == 0);
  assert (ai_model == NULL && ai_num_predict == 0);
  assert (evaluate ("(ai-complete-region \"test-model\" 3)") == TRUE);
  assert (strcmp (ai_prompt, "h\xc3\xa9llo\n") == 0);
  assert (strcmp (ai_model, "test-model") == 0);
  assert (ai_num_predict == 3);
  assert (evaluate ("(ai-complete-region \"\")") == FALSE);
  assert (evaluate ("(ai-complete-region \"model\" 0)") == FALSE);
  assert (evaluate ("(ai-complete-region \"model\" 3 \"extra\")")
          == FALSE);
  getregion_status = FALSE;
  lisp_message[0] = 0;
  assert (evaluate ("(ai-complete-region)") == TRUE);
  assert (strcmp (lisp_message, "No mark in this window") == 0);
  getregion_status = TRUE;

  {
    char result[LISP_OUTPUT_SIZE];

    assert (unsetenv ("GPUSTACK_API_KEY") == 0);
    assert (setenv ("OPENAI_API_KEY", "fallback-key", 1) == 0);
    ai_command_defined = FALSE;
    ai_region_command_defined = FALSE;
    bound_key = 0;
    bound_symbol = NULL;
    bound_line_key = 0;
    bound_region_key = 0;
    assert (evaluate_example_file ("examples/ai-assist-ollama.l",
                                   result, sizeof (result)) == TRUE);
    assert (bound_line_key == (KMETA | 'A'));
    assert (bound_region_key == (KCTLX | 'A'));
    bound_key = 0;
    bound_symbol = NULL;
    bound_line_key = 0;
    bound_region_key = 0;
    assert (evaluate_example_file ("examples/ai-assist-gpustack.l",
                                   result, sizeof (result)) == TRUE);
    assert (bound_line_key == (KMETA | 'A'));
    assert (bound_region_key == (KCTLX | 'A'));
    assert (strcmp (ai_reasoning_effort, "low") == 0);
    assert (ai_timeout == 180);
    assert (strcmp (ai_api_key, "fallback-key") == 0);

    assert (setenv ("GPUSTACK_API_KEY", "primary-key", 1) == 0);
    ai_command_defined = FALSE;
    ai_region_command_defined = FALSE;
    bound_key = 0;
    bound_symbol = NULL;
    bound_line_key = 0;
    bound_region_key = 0;
    assert (evaluate_example_file ("examples/ai-assist-gpustack.l",
                                   result, sizeof (result)) == TRUE);
    assert (bound_line_key == (KMETA | 'A'));
    assert (bound_region_key == (KCTLX | 'A'));
    assert (strcmp (ai_api_key, "primary-key") == 0);
    assert (unsetenv ("GPUSTACK_API_KEY") == 0);
    assert (unsetenv ("OPENAI_API_KEY") == 0);

    lisp_dired_defined = FALSE;
    dired_refresh_defined = FALSE;
    dired_find_file_defined = FALSE;
    dired_up_directory_defined = FALSE;
    dired_quit_defined = FALSE;
    bound_key = 0;
    bound_symbol = NULL;
    assert (evaluate_example_file ("lisp/dired.l",
                                   result, sizeof (result)) == TRUE);
    assert (lisp_dired_defined && dired_refresh_defined
            && dired_find_file_defined && dired_up_directory_defined
            && dired_quit_defined);
    assert (bound_key == (KCTLX | 'D'));
    assert (bound_symbol == &lisp_dired_symbol);

    {
      char entry[128];

      assert (rmdir ("/tmp/microemacs-dired-test/parent") == 0
              || errno == ENOENT);
      assert (rmdir ("/tmp/microemacs-dired-test") == 0
              || errno == ENOENT);
      assert (mkdir ("/tmp/microemacs-dired-test", 0755) == 0);
      assert (mkdir ("/tmp/microemacs-dired-test/parent", 0755) == 0);
      snprintf (entry, sizeof (entry),
                "%-10s %3llu %8lld %16s  %s",
                "drwxr-xr-x", 2ULL, 4096LL,
                "2026-10-15 10:00", "parent/");
      set_primitive_line (entry);
      strcpy (primitive_buffer.b_fname, "/tmp/microemacs-dired-test/parent");
      inserted_text[0] = 0;
      assert (tiny_lisp_eval ("(dired-up-directory)",
                              result, sizeof (result)) == 1);
      assert (strcmp (result,
                      "\"Dired: Enter opens, ^ goes up, g refreshes, q quits\"")
              == 0);
      assert (strcmp (dired_buffer.b_fname,
                      "/tmp/microemacs-dired-test") == 0);
      assert (strncmp (inserted_text, "/tmp/microemacs-dired-test:\n",
                       strlen ("/tmp/microemacs-dired-test:\n")) == 0);
      assert (bound_mode_key == 'q');
      assert (bound_mode_symbol == &dired_quit_symbol);
      assert (evaluate ("(define-mode-key \"^\" \"dired-up-directory\")")
              == TRUE);
      assert (bound_mode_key == '^');
      assert (bound_mode_symbol == &dired_up_directory_symbol);
      assert (evaluate ("(define-mode-key \"q\" \"dired-quit\")")
              == TRUE);
      assert (bound_mode_key == 'q');
      assert (bound_mode_symbol == &dired_quit_symbol);
      back_buffer_called = 0;
      assert (tiny_lisp_eval ("(dired-quit)", result, sizeof (result)) == 1);
      assert (strcmp (result, "t") == 0);
      assert (back_buffer_called == 1);
      assert (rmdir ("/tmp/microemacs-dired-test/parent") == 0);
      assert (rmdir ("/tmp/microemacs-dired-test") == 0);
      curbp = &primitive_buffer;
      curwp = &primitive_window;
    }

    {
      FILE *file = fopen ("/tmp/microemacs-dired-open.txt", "wb");
      char entry[128];

      assert (file != NULL);
      assert (fclose (file) == 0);
      strcpy (primitive_buffer.b_fname, "/tmp");
      snprintf (entry, sizeof (entry),
                "%-10s %3llu %8lld %16s  %s",
                "-rw-r--r--", 1ULL, 0LL,
                "2026-10-15 10:00", "microemacs-dired-open.txt");
      set_primitive_line (entry);
      assert (tiny_lisp_eval ("(dired-find-file)",
                              result, sizeof (result)) == 1);
      assert (strcmp (visited_file,
                      "/tmp/microemacs-dired-open.txt") == 0);
      assert (remove ("/tmp/microemacs-dired-open.txt") == 0);
    }

    smart_find_file_defined = FALSE;
    bound_key = 0;
    bound_symbol = NULL;
    assert (evaluate_example_file ("lisp/smart-find-file.l",
                                   result, sizeof (result)) == TRUE);
    assert (smart_find_file_defined);
    assert (bound_key == (KCTLX | KCTRL | 'F'));
    assert (bound_symbol == &smart_find_file_symbol);

    assert (evaluate ("(find-file \"/nonexistent-smart-file.c\")") == TRUE);
    assert (strcmp (visited_file, "/nonexistent-smart-file.c") == 0);
  }

  puts ("lisp tests passed");
  return 0;
}
