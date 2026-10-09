#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "../syntax.c"

int colorflag = TRUE;
BUFFER *curbp;
EWINDOW *wheadp;
int sgarbf;

void
eprintf (const char *format, ...)
{
  (void) format;
}

#define OUTPUT_MAX 256

static unsigned int output_chars[OUTPUT_MAX];
static int output_colors[OUTPUT_MAX];
static int output_len;

void
vtputc_color (unsigned int c, int color)
{
  assert (output_len < OUTPUT_MAX);
  output_chars[output_len] = c;
  output_colors[output_len] = color;
  ++output_len;
}

static const SYNTAX *
syntax_for_extension (const char *extension)
{
  char name[32];

  snprintf (name, sizeof (name), "test%s", extension);
  return syntax_for_name (name);
}

static void
render_line (const SYNTAX *syntax, const char *line,
             int *state)
{
  output_len = 0;
  syntax_line (syntax, (const uchar *) line, (int) strlen (line), state,
               TRUE);
}

static void
expect_length (int length)
{
  if (output_len != length)
    {
      fprintf (stderr, "expected %d rendered characters, got %d\n",
               length, output_len);
      assert (FALSE);
    }
}

static void
expect_color_at (int index, int color)
{
  if (output_colors[index] != color)
    {
      fprintf (stderr, "expected color %d at %d, got %d\n",
               color, index, output_colors[index]);
      assert (FALSE);
    }
}

static void
expect_all_colors (int color)
{
  int index;

  for (index = 0; index < output_len; ++index)
    expect_color_at (index, color);
}

static const SYNTAX *
syntax_for_shebang (const char *text)
{
  BUFFER buffer;
  LINE header;
  LINE *line;
  size_t text_len = strlen (text);
  const SYNTAX *syntax;

  line = calloc (1, LINEHDR_SIZE + text_len);
  assert (line != NULL);
  line->l_size = (int) text_len;
  line->l_used = (int) text_len;
  memcpy (lgets (line), text, text_len);

  memset (&buffer, 0, sizeof (buffer));
  memset (&header, 0, sizeof (header));
  strcpy (buffer.b_fname, "script");
  buffer.b_linep = &header;
  lforw (&header) = line;
  lback (&header) = line;
  lforw (line) = &header;
  lback (line) = &header;

  syntax = syntax_for_buffer (&buffer);
  free (line);
  return syntax;
}

int
main (void)
{
  const SYNTAX *cpp = syntax_for_extension (".cpp");
  const SYNTAX *lisp = syntax_for_extension (".lisp");
  const SYNTAX *elisp = syntax_for_extension (".el");
  const SYNTAX *c_syntax = syntax_for_extension (".c");
  const SYNTAX *bash = syntax_for_extension (".sh");
  BUFFER buffer;
  LINE header;
  int state = SYNTAX_STATE_NONE;

  assert (cpp != NULL && lisp != NULL && elisp != NULL
          && c_syntax != NULL && bash != NULL);
  assert (syntax_for_name (".C") == cpp);
  assert (syntax_for_name (".c") == c_syntax);
  assert (syntax_for_name (".profile") == bash);
  assert (syntax_for_name (".emacs") == elisp);
  assert (syntax_for_shebang ("#!/usr/bin/sbcl --script") == lisp);
  assert (syntax_for_shebang ("#!/usr/bin/emacs --script") == elisp);

  memset (&buffer, 0, sizeof (buffer));
  memset (&header, 0, sizeof (header));
  strcpy (buffer.b_fname, "test.c");
  buffer.b_linep = &header;
  lforw (&header) = &header;
  lback (&header) = &header;
  assert (syntax_for_buffer (&buffer) == c_syntax);
  assert (strcmp (syntax_name_for_buffer (&buffer), "C") == 0);
  assert (syntax_set_buffer (&buffer, "cpp") == TRUE);
  assert (syntax_for_buffer (&buffer) == cpp);
  assert (strcmp (syntax_name_for_buffer (&buffer), "C++") == 0);
  assert (syntax_set_buffer (&buffer, "c++") == TRUE);
  assert (syntax_for_buffer (&buffer) == cpp);
  assert (syntax_set_buffer (&buffer, "text") == TRUE);
  assert (syntax_for_buffer (&buffer) == NULL);
  assert (syntax_name_for_buffer (&buffer) == NULL);
  syntax_clear_buffer (&buffer);
  assert (syntax_for_buffer (&buffer) == c_syntax);

  strcpy (buffer.b_fname, ".emacs");
  assert (syntax_for_buffer (&buffer) == elisp);
  assert (strcmp (syntax_name_for_buffer (&buffer), "Emacs Lisp") == 0);
  strcpy (buffer.b_fname, "1.sh");
  assert (syntax_for_buffer (&buffer) == bash);
  assert (strcmp (syntax_name_for_buffer (&buffer), "Bash") == 0);
  colorflag = FALSE;
  assert (syntax_for_buffer (&buffer) == NULL);
  assert (syntax_name_for_buffer (&buffer) == NULL);
  assert (syntax_set_buffer (&buffer, "cpp") == FALSE);
  curbp = &buffer;
  assert (cmode (FALSE, 1, 0) == FALSE);
  assert (textmode (FALSE, 1, 0) == FALSE);
  assert (buffer.b_syntax_explicit == FALSE);
  colorflag = TRUE;
  assert (cmode (FALSE, 1, 0) == TRUE);
  assert (syntax_for_buffer (&buffer) == c_syntax);
  colorflag = FALSE;
  assert (syntax_for_buffer (&buffer) == NULL);
  assert (syntax_name_for_buffer (&buffer) == NULL);
  colorflag = TRUE;
  assert (syntax_for_buffer (&buffer) == c_syntax);
  syntax_clear_buffer (&buffer);
  assert (syntax_set_buffer (&buffer, "text") == TRUE);
  assert (syntax_for_buffer (&buffer) == NULL);
  assert (syntax_name_for_buffer (&buffer) == NULL);

  render_line (lisp, "; comment", &state);
  expect_length (9);
  expect_all_colors (CCOMMENT);
  assert (state == SYNTAX_STATE_NONE);

  render_line (lisp, "\"; string\"", &state);
  expect_length (10);
  expect_all_colors (CSTRING);
  assert (state == SYNTAX_STATE_NONE);

  render_line (lisp, "(defun foo nil :key #'car)", &state);
  expect_length (26);
  expect_color_at (1, CKEYWORD);
  expect_color_at (7, CTEXT);
  expect_color_at (11, CKEYWORD);
  expect_color_at (15, CPREPROC);
  expect_color_at (20, CPREPROC);
  expect_color_at (22, CTEXT);
  assert (state == SYNTAX_STATE_NONE);

  render_line (lisp, "#| outer #| inner", &state);
  expect_length (17);
  expect_all_colors (CCOMMENT);
  assert (state == SYNTAX_STATE_LISP_BLOCK_COMMENT + 1);

  render_line (lisp, "still |# comment", &state);
  expect_length (16);
  expect_all_colors (CCOMMENT);
  assert (state == SYNTAX_STATE_LISP_BLOCK_COMMENT);

  render_line (lisp, "end |# code", &state);
  expect_length (11);
  expect_color_at (4, CCOMMENT);
  expect_color_at (5, CCOMMENT);
  expect_color_at (6, CTEXT);
  expect_color_at (7, CTEXT);
  assert (state == SYNTAX_STATE_NONE);

  render_line (elisp, "(defun foo) ; comment", &state);
  expect_length (21);
  expect_color_at (1, CKEYWORD);
  expect_color_at (7, CTEXT);
  expect_color_at (12, CCOMMENT);
  expect_color_at (20, CCOMMENT);
  assert (state == SYNTAX_STATE_NONE);

  state = SYNTAX_STATE_NONE;
  render_line (c_syntax, "/* comment */ int x;", &state);
  expect_length (20);
  expect_color_at (12, CCOMMENT);
  expect_color_at (13, CTEXT);
  expect_color_at (14, CKEYWORD);
  expect_color_at (16, CKEYWORD);
  expect_color_at (17, CTEXT);
  assert (state == SYNTAX_STATE_NONE);

  puts ("syntax tests passed");
  return 0;
}
