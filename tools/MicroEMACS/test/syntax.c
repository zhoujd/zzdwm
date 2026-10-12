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

static void
expect_color_range (int start, int end, int color)
{
  int index;

  for (index = start; index < end; ++index)
    expect_color_at (index, color);
}

static const SYNTAX *
syntax_for_first_line (const char *text)
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
  const SYNTAX *scheme = syntax_for_extension (".scm");
  const SYNTAX *c_syntax = syntax_for_extension (".c");
  const SYNTAX *bash = syntax_for_extension (".sh");
  const SYNTAX *json = syntax_for_extension (".json");
  const SYNTAX *yaml = syntax_for_extension (".yaml");
  const SYNTAX *toml = syntax_for_extension (".toml");
  const SYNTAX *cmake = syntax_for_extension (".cmake");
  const SYNTAX *makefile = syntax_for_extension (".mk");
  const SYNTAX *sql = syntax_for_filetype ("sql");
  BUFFER buffer;
  LINE header;
  int state = SYNTAX_STATE_NONE;

  assert (cpp != NULL && lisp != NULL && elisp != NULL
          && scheme != NULL
          && c_syntax != NULL && bash != NULL && json != NULL
          && yaml != NULL && toml != NULL && cmake != NULL
          && makefile != NULL && sql != NULL);
  assert (syntax_for_name (".C") == cpp);
  assert (syntax_for_name (".c") == c_syntax);
  assert (syntax_for_name (".profile") == bash);
  assert (syntax_for_name (".emacs") == elisp);
  assert (syntax_for_name ("test.ss") == scheme);
  assert (syntax_for_name ("CMakeLists.txt") == cmake);
  assert (syntax_for_name ("Makefile") == makefile);
  assert (syntax_for_name ("test.sql") == sql);
  assert (syntax_for_name ("test.SQL") == sql);
  assert (syntax_for_first_line ("#!/usr/bin/sbcl --script") == lisp);
  assert (syntax_for_first_line ("#!/usr/bin/emacs --script") == elisp);
  assert (syntax_for_first_line ("#!/usr/bin/guile --no-auto-compile")
          == scheme);
  assert (syntax_for_first_line
          ("# Makefile - makefile input file for MicroEMACS.") == makefile);
  assert (syntax_for_first_line ("# makefile input file") == makefile);
  assert (syntax_for_first_line ("# editor settings") == NULL);

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
  assert (syntax_set_buffer (&buffer, "json") == TRUE);
  assert (syntax_for_buffer (&buffer) == json);
  assert (strcmp (syntax_name_for_buffer (&buffer), "JSON") == 0);
  assert (syntax_set_buffer (&buffer, "yaml") == TRUE);
  assert (syntax_for_buffer (&buffer) == yaml);
  assert (syntax_set_buffer (&buffer, "toml") == TRUE);
  assert (syntax_for_buffer (&buffer) == toml);
  assert (syntax_set_buffer (&buffer, "cmake") == TRUE);
  assert (syntax_for_buffer (&buffer) == cmake);
  assert (syntax_set_buffer (&buffer, "makefile") == TRUE);
  assert (syntax_for_buffer (&buffer) == makefile);
  assert (syntax_set_buffer (&buffer, "scheme") == TRUE);
  assert (syntax_for_buffer (&buffer) == scheme);
  assert (strcmp (syntax_name_for_buffer (&buffer), "Scheme") == 0);
  assert (syntax_set_buffer (&buffer, "sql") == TRUE);
  assert (syntax_for_buffer (&buffer) == sql);
  assert (strcmp (syntax_name_for_buffer (&buffer), "SQL") == 0);
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
  assert (sqlmode (FALSE, 1, 0) == TRUE);
  assert (syntax_for_buffer (&buffer) == sql);
  assert (strcmp (syntax_name_for_buffer (&buffer), "SQL") == 0);
  syntax_clear_buffer (&buffer);
  assert (syntax_for_buffer (&buffer) == bash);
  colorflag = FALSE;
  assert (syntax_for_buffer (&buffer) == NULL);
  assert (syntax_name_for_buffer (&buffer) == NULL);
  colorflag = TRUE;
  assert (syntax_for_buffer (&buffer) == bash);
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

  render_line (scheme, "(define x) #t", &state);
  expect_length (13);
  expect_color_at (1, CKEYWORD);
  expect_color_at (8, CTEXT);
  expect_color_at (11, CPREPROC);
  expect_color_at (12, CPREPROC);
  assert (state == SYNTAX_STATE_NONE);

  render_line (sql, "SELECT name FROM users", &state);
  expect_length (22);
  expect_color_at (0, CKEYWORD);
  expect_color_at (7, CTEXT);
  expect_color_at (12, CKEYWORD);
  expect_color_at (17, CTEXT);
  assert (state == SYNTAX_STATE_NONE);

  render_line (sql, "-- comment", &state);
  expect_length (10);
  expect_all_colors (CCOMMENT);
  assert (state == SYNTAX_STATE_NONE);

  render_line (sql, "'text'", &state);
  expect_length (6);
  expect_all_colors (CSTRING);
  assert (state == SYNTAX_STATE_NONE);

  render_line (sql, "/* outer", &state);
  expect_length (8);
  expect_all_colors (CCOMMENT);
  assert (state == SYNTAX_STATE_C_COMMENT);

  render_line (sql, "still */ code", &state);
  expect_length (13);
  expect_color_at (0, CCOMMENT);
  expect_color_at (7, CCOMMENT);
  expect_color_at (8, CTEXT);
  expect_color_at (9, CTEXT);
  assert (state == SYNTAX_STATE_NONE);

  render_line (scheme, "; comment", &state);
  expect_length (9);
  expect_all_colors (CCOMMENT);
  assert (state == SYNTAX_STATE_NONE);

  render_line (scheme, "#| outer #| inner", &state);
  expect_length (17);
  expect_all_colors (CCOMMENT);
  assert (state == SYNTAX_STATE_LISP_BLOCK_COMMENT + 1);

  state = SYNTAX_STATE_NONE;
  render_line (json, "{\"name\": true}", &state);
  expect_length (14);
  expect_color_at (0, CTEXT);
  expect_color_at (1, CPREPROC);
  expect_color_at (6, CPREPROC);
  expect_color_at (7, CTEXT);
  expect_color_at (9, CKEYWORD);
  expect_color_at (12, CKEYWORD);
  expect_color_at (13, CTEXT);
  assert (state == SYNTAX_STATE_NONE);

  render_line (yaml, "key: value # comment", &state);
  expect_length (20);
  expect_color_at (0, CPREPROC);
  expect_color_at (2, CPREPROC);
  expect_color_at (3, CTEXT);
  expect_color_at (5, CTEXT);
  expect_color_at (10, CTEXT);
  expect_color_at (11, CCOMMENT);
  expect_color_at (19, CCOMMENT);
  assert (state == SYNTAX_STATE_NONE);

  render_line (yaml, "image: ollama/ollama:latest", &state);
  expect_length (27);
  expect_color_range (0, 5, CPREPROC);
  expect_color_range (5, 27, CTEXT);

  render_line (yaml, "      - /opt/tool/ollama:/root/.ollama", &state);
  expect_length (38);
  expect_all_colors (CTEXT);

  render_line (yaml, "image: ghcr.io/open-webui/open-webui:main", &state);
  expect_length (41);
  expect_color_range (0, 5, CPREPROC);
  expect_color_range (5, 41, CTEXT);

  render_line (yaml, "      - /opt/tool/open-webui:/app/backend/data", &state);
  expect_length (46);
  expect_all_colors (CTEXT);

  render_line (yaml, "      - OLLAMA_BASE_URL=http://ollama:11434", &state);
  expect_length (43);
  expect_all_colors (CTEXT);

  render_line (yaml, "  - key: value", &state);
  expect_length (14);
  expect_color_range (4, 7, CPREPROC);
  expect_color_range (7, 14, CTEXT);

  render_line (toml, "[package]", &state);
  expect_length (9);
  expect_all_colors (CPREPROC);
  render_line (toml, "name = \"value\"", &state);
  expect_length (14);
  expect_color_at (0, CPREPROC);
  expect_color_at (3, CPREPROC);
  expect_color_at (5, CTEXT);
  expect_color_at (7, CSTRING);
  expect_color_at (13, CSTRING);
  assert (state == SYNTAX_STATE_NONE);

  render_line (cmake, "set(VERSION ${PROJECT_VERSION}) # comment", &state);
  expect_length (41);
  expect_color_at (0, CKEYWORD);
  expect_color_at (2, CKEYWORD);
  expect_color_at (4, CTEXT);
  expect_color_at (12, CPREPROC);
  expect_color_at (29, CPREPROC);
  expect_color_at (32, CCOMMENT);
  assert (state == SYNTAX_STATE_NONE);

  render_line (cmake, "#[=[ multi", &state);
  expect_length (10);
  expect_all_colors (CCOMMENT);
  assert (state == SYNTAX_STATE_CMAKE_BRACKET_COMMENT + 1);
  render_line (cmake, "still ]=] code", &state);
  expect_length (14);
  expect_color_at (7, CCOMMENT);
  expect_color_at (8, CCOMMENT);
  expect_color_at (9, CTEXT);
  expect_color_at (10, CTEXT);
  assert (state == SYNTAX_STATE_NONE);

  render_line (makefile, "all: build", &state);
  expect_length (10);
  expect_color_at (0, CPREPROC);
  expect_color_at (2, CPREPROC);
  expect_color_at (3, CTEXT);
  expect_color_at (5, CTEXT);
  render_line (makefile, ".PHONY: all", &state);
  expect_length (11);
  expect_color_at (0, CPREPROC);
  expect_color_at (5, CPREPROC);
  expect_color_at (6, CTEXT);
  expect_color_at (8, CTEXT);
  render_line (makefile, "CC = gcc", &state);
  expect_length (8);
  expect_color_at (0, CPREPROC);
  expect_color_at (1, CPREPROC);
  expect_color_at (3, CTEXT);
  expect_color_at (5, CTEXT);
  render_line (makefile, "\tgcc -O2", &state);
  expect_length (8);
  expect_all_colors (CPREPROC);
  render_line (makefile, "$(VAR)", &state);
  expect_length (6);
  expect_all_colors (CPREPROC);
  render_line (makefile, "include Makefile", &state);
  expect_length (16);
  expect_color_at (0, CKEYWORD);
  expect_color_at (6, CKEYWORD);
  expect_color_at (8, CTEXT);
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
