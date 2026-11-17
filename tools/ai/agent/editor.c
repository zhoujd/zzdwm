/* Multiline editor: a shared buffer model (used by the agent prompt on a
   terminal and by the TUI input window), the raw-mode terminal reader
   that renders it with ANSI cursor moves, and the standalone `editor`
   subcommand. */

#define _XOPEN_SOURCE 600

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>

#include "utf.h"

#include "editor.h"

/* --- shared buffer model ----------------------------------------- */

void
editor_init (EditorBuffer *editor, char *text, size_t capacity)
{
  editor->text = text;
  editor->capacity = capacity;
  editor->length = 0;
  editor->cursor = 0;
  if (capacity != 0)
    text[0] = 0;
}

size_t
editor_line_start (const EditorBuffer *editor, size_t position)
{
  while (position > 0 && editor->text[position - 1] != '\n')
    position--;
  return position;
}

size_t
editor_line_end (const EditorBuffer *editor, size_t position)
{
  while (position < editor->length && editor->text[position] != '\n')
    position++;
  return position;
}

/* Start of the rune that ends at `position`. */
size_t
editor_rune_start (const EditorBuffer *editor, size_t position)
{
  size_t index = 0;
  size_t last_start = 0;

  while (index < position)
    {
      Rune rune;
      int rune_length = chartorune (&rune, editor->text + index);

      last_start = index;
      index += (size_t)(rune_length > 0 ? rune_length : 1);
    }
  return last_start;
}

/* Where C-W would cut: back over trailing blanks, then the word itself,
   never past the start of the line. */
size_t
editor_word_start (const EditorBuffer *editor)
{
  size_t line_start = editor_line_start (editor, editor->cursor);
  size_t start = editor->cursor;

  while (start > line_start && editor->text[start - 1] == ' ')
    start = editor_rune_start (editor, start);
  while (start > line_start && editor->text[start - 1] != ' ')
    start = editor_rune_start (editor, start);
  return start;
}

/* Display width in terminal cells of `length` bytes of UTF-8 text. */
int
editor_span_width (const char *text, size_t length)
{
  size_t index = 0;
  int width = 0;

  while (index < length)
    {
      Rune rune;
      int rune_length = chartorune (&rune, (char *)text + index);
      int rune_width = wcwidth ((wchar_t)rune);

      if (rune_width < 0)
        rune_width = 1;
      width += rune_width;
      index += (size_t)(rune_length > 0 ? rune_length : 1);
    }
  return width;
}

void
editor_move_backward (EditorBuffer *editor)
{
  if (editor->cursor != 0)
    editor->cursor = editor_rune_start (editor, editor->cursor);
}

void
editor_move_forward (EditorBuffer *editor)
{
  if (editor->cursor < editor->length)
    {
      Rune rune;
      int rune_length = chartorune (&rune, editor->text + editor->cursor);

      editor->cursor += (size_t)(rune_length > 0 ? rune_length : 1);
    }
}

void
editor_move_line_start (EditorBuffer *editor)
{
  editor->cursor = editor_line_start (editor, editor->cursor);
}

void
editor_move_line_end (EditorBuffer *editor)
{
  editor->cursor = editor_line_end (editor, editor->cursor);
}

/* C-P: up one line, keeping the column when the line above is long
   enough; the column steps back to a rune boundary so multibyte text
   never splits a rune. */
void
editor_move_line_up (EditorBuffer *editor)
{
  size_t start;
  size_t end;
  size_t column;

  end = editor_line_start (editor, editor->cursor);
  if (end == 0)
    return;
  end--;
  start = editor_line_start (editor, end);
  column = editor->cursor - end - 1;
  if (column > end - start)
    column = end - start;
  while (column > 0 && ((unsigned char)editor->text[start + column] & 0xc0) == 0x80)
    column--;
  editor->cursor = start + column;
}

/* C-N: down one line with the same column rule. */
void
editor_move_line_down (EditorBuffer *editor)
{
  size_t end = editor_line_end (editor, editor->cursor);
  size_t next_end;
  size_t column;

  if (end >= editor->length)
    return;
  next_end = editor_line_end (editor, end + 1);
  column = editor->cursor - editor_line_start (editor, editor->cursor);
  if (column > next_end - end - 1)
    column = next_end - end - 1;
  while (column > 0 && ((unsigned char)editor->text[end + 1 + column] & 0xc0) == 0x80)
    column--;
  editor->cursor = end + 1 + column;
}

int
editor_insert (EditorBuffer *editor, const char *bytes, size_t length)
{
  if (length == 0 || editor->length + length >= editor->capacity)
    return -1;
  memmove (editor->text + editor->cursor + length, editor->text + editor->cursor,
           editor->length - editor->cursor);
  memcpy (editor->text + editor->cursor, bytes, length);
  editor->cursor += length;
  editor->length += length;
  editor->text[editor->length] = 0;
  return 0;
}

size_t
editor_delete_backward (EditorBuffer *editor)
{
  size_t start;
  size_t removed;

  if (editor->cursor == 0)
    return 0;
  start = editor_rune_start (editor, editor->cursor);
  removed = editor->cursor - start;
  memmove (editor->text + start, editor->text + editor->cursor, editor->length - editor->cursor);
  editor->length -= removed;
  editor->cursor = start;
  editor->text[editor->length] = 0;
  return removed;
}

size_t
editor_delete_forward (EditorBuffer *editor)
{
  Rune rune;
  int rune_length;
  size_t removed;

  if (editor->cursor >= editor->length)
    return 0;
  rune_length = chartorune (&rune, editor->text + editor->cursor);
  removed = (size_t)(rune_length > 0 ? rune_length : 1);
  memmove (editor->text + editor->cursor, editor->text + editor->cursor + removed,
           editor->length - editor->cursor - removed);
  editor->length -= removed;
  editor->text[editor->length] = 0;
  return removed;
}

size_t
editor_delete_word (EditorBuffer *editor)
{
  size_t start = editor_word_start (editor);
  size_t removed = editor->cursor - start;

  if (removed == 0)
    return 0;
  memmove (editor->text + start, editor->text + editor->cursor, editor->length - editor->cursor);
  editor->length -= removed;
  editor->cursor = start;
  editor->text[editor->length] = 0;
  return removed;
}

size_t
editor_kill_line_end (EditorBuffer *editor)
{
  size_t end = editor_line_end (editor, editor->cursor);
  size_t removed = end - editor->cursor;

  if (removed == 0)
    return 0;
  memmove (editor->text + editor->cursor, editor->text + end, editor->length - end);
  editor->length -= removed;
  editor->text[editor->length] = 0;
  return removed;
}

void
editor_clear (EditorBuffer *editor)
{
  editor->length = 0;
  editor->cursor = 0;
  editor->text[0] = 0;
}

/* --- terminal rendering ------------------------------------------ */

/* Prompts and editor errors go through the host's printer so it can
   colorize labels like [You]; the default writes them plainly. */
static void (*prompt_printer) (const char *text);

static int write_all (int file_descriptor, const char *data, size_t length);

void
editor_set_prompt_printer (void (*printer) (const char *text))
{
  prompt_printer = printer;
}

static void
print_prompt (const char *text)
{
  if (prompt_printer != NULL)
    prompt_printer (text);
  else
    write_all (STDOUT_FILENO, text, strlen (text));
}

static int
write_all (int file_descriptor, const char *data, size_t length)
{
  while (length > 0)
    {
      ssize_t written = write (file_descriptor, data, length);

      if (written < 0 && errno == EINTR)
        continue;
      if (written <= 0)
        return -1;
      data += written;
      length -= (size_t)written;
    }
  return 0;
}

static void
write_repeated (const char *data, size_t count)
{
  while (count-- != 0)
    write_all (STDOUT_FILENO, data, 1);
}

/* Blank `count` screen cells at the cursor and return to the first one. */
static void
erase_cells (size_t count)
{
  write_repeated (" ", count);
  write_repeated ("\b", count);
}

/* After an edit shifted the text left of the cursor, reprint the tail,
   blank the cells the removed runes still occupy, and park the cursor
   back at the edit point. */
static void
redraw_line_tail (const char *buffer, size_t cursor, size_t length, size_t removed)
{
  write_all (STDOUT_FILENO, buffer + cursor, length - cursor);
  erase_cells (removed);
  write_repeated ("\b", length - cursor);
}

/* The prompt as rendered on screen: separators like the leading newline
   of "\n[You]: " belong to the surrounding output, not to a reprint. */
static const char *
prompt_display (const char *prompt)
{
  if (prompt == NULL)
    return "";
  while (prompt[0] == '\n')
    prompt++;
  return prompt;
}

/* Step the cursor down to the last input row before printing below the
   input again (submitting, or opening a new line from an upper line). */
static void
move_to_last_row (const char *buffer, size_t cursor, size_t length)
{
  size_t rows = 0;
  size_t index;

  for (index = cursor; index < length; index++)
    if (buffer[index] == '\n')
      rows++;
  while (rows-- != 0)
    write_all (STDOUT_FILENO, "\033[B", 3);
}

/* Read one multiline input on a terminal. Enter (CR) submits and returns
   0; C-J opens another line; Emacs-style keys edit within the current
   line or walk between lines. Returns -1 on EOF, error, or when C-D ends
   an empty input. */
int
editor_read (const char *prompt, const char *continuation_prompt, char *buffer, size_t capacity)
{
  EditorBuffer editor;
  struct termios original_terminal;
  struct termios raw_terminal;
  int escape_state = 0;

  editor_init (&editor, buffer, capacity);
  if (tcgetattr (STDIN_FILENO, &original_terminal) != 0)
    return -1;
  raw_terminal = original_terminal;
  /* Clearing ICRNL keeps Enter (CR) distinct from C-J (NL); clearing
     ICANON and ECHO hands line editing and echo to this function. */
  raw_terminal.c_lflag &= ~(ICANON | ECHO | ECHONL);
  raw_terminal.c_iflag &= ~(ICRNL | INLCR | IGNCR);
  raw_terminal.c_cc[VMIN] = 1;
  raw_terminal.c_cc[VTIME] = 0;
  if (tcsetattr (STDIN_FILENO, TCSANOW, &raw_terminal) != 0)
    return -1;
  /* The prompt is printed after the switch so its appearance means the
     keyboard protocol is already in effect. */
  if (prompt != NULL && prompt[0] != 0)
    print_prompt (prompt);
  for (;;)
    {
      char character;
      ssize_t received = read (STDIN_FILENO, &character, 1);
      size_t line_start = editor_line_start (&editor, editor.cursor);

      if (received != 1)
        {
          tcsetattr (STDIN_FILENO, TCSANOW, &original_terminal);
          return received == 0 && editor.length != 0 ? 0 : -1;
        }
      if (escape_state == 1)
        {
          escape_state = character == '[' || character == 'O' ? 2 : 0;
          continue;
        }
      if (escape_state == 2)
        {
          if ((unsigned char)character >= 0x40)
            escape_state = 0;
          continue;
        }
      if (character == 4)
        {
          /* C-D deletes forward like the TUI, staying on the current
             line; at the end of the whole input it submits what is
             typed, as in a shell, and ends an empty conversation like
             EOF. */
          size_t end = editor_line_end (&editor, editor.cursor);

          if (editor.cursor < end)
            {
              size_t width = (size_t)editor_span_width (editor.text + editor.cursor, end - editor.cursor);
              size_t removed = editor_delete_forward (&editor);

              redraw_line_tail (editor.text, editor.cursor, end - removed, width);
              continue;
            }
          if (editor.cursor == editor.length)
            {
              if (editor.length == 0)
                {
                  tcsetattr (STDIN_FILENO, TCSANOW, &original_terminal);
                  return -1;
                }
              tcsetattr (STDIN_FILENO, TCSANOW, &original_terminal);
              move_to_last_row (editor.text, editor.cursor, editor.length);
              write_all (STDOUT_FILENO, "\n", 1);
              return 0;
            }
          continue;
        }
      if (character == '\r')
        {
          /* Enter submits from the last input row so nothing above it
             is left dangling beside the response. */
          tcsetattr (STDIN_FILENO, TCSANOW, &original_terminal);
          move_to_last_row (editor.text, editor.cursor, editor.length);
          write_all (STDOUT_FILENO, "\n", 1);
          return 0;
        }
      if (character == '\n')
        {
          /* C-J starts a new line within the prompt; when the cursor
             sits on an earlier line it steps down to the last one
             first. */
          move_to_last_row (editor.text, editor.cursor, editor.length);
          editor.cursor = editor.length;
          if (editor_insert (&editor, "\n", 1) < 0)
            {
              print_prompt ("\r\nerror: input is too long\n");
              tcsetattr (STDIN_FILENO, TCSANOW, &original_terminal);
              return -1;
            }
          line_start = editor.cursor;
          write_all (STDOUT_FILENO, "\r\n", 2);
          if (continuation_prompt != NULL)
            print_prompt (continuation_prompt);
          continue;
        }
      if (character == 127 || character == 8)
        {
          if (editor.cursor > line_start)
            {
              size_t end = editor_line_end (&editor, line_start);
              size_t start = editor_rune_start (&editor, editor.cursor);
              size_t width = (size_t)editor_span_width (editor.text + start, editor.cursor - start);
              size_t removed = editor_delete_backward (&editor);

              write_repeated ("\b", width);
              redraw_line_tail (editor.text, editor.cursor, end - removed, width);
            }
          continue;
        }
      if (character == 1 || character == 2 || character == 5 || character == 6)
        {
          /* C-A/C-E move to the line ends, C-B/C-F one rune back/forward;
             moving right reprints the rune under the cursor, which the
             terminal renders identically. */
          if (character == 1)
            {
              write_repeated ("\b", (size_t)editor_span_width (editor.text + line_start,
                                                              editor.cursor - line_start));
              editor_move_line_start (&editor);
            }
          else if (character == 2)
            {
              if (editor.cursor > line_start)
                {
                  size_t start = editor_rune_start (&editor, editor.cursor);
                  size_t width = (size_t)editor_span_width (editor.text + start, editor.cursor - start);

                  write_repeated ("\b", width);
                  editor_move_backward (&editor);
                }
            }
          else if (character == 5)
            {
              size_t end = editor_line_end (&editor, editor.cursor);

              write_all (STDOUT_FILENO, editor.text + editor.cursor, end - editor.cursor);
              editor_move_line_end (&editor);
            }
          else
            {
              if (editor.cursor < editor.length && editor.text[editor.cursor] != '\n')
                {
                  Rune rune;
                  int rune_length = chartorune (&rune, editor.text + editor.cursor);

                  write_all (STDOUT_FILENO, editor.text + editor.cursor,
                             (size_t)(rune_length > 0 ? rune_length : 1));
                  editor_move_forward (&editor);
                }
            }
          continue;
        }
      if (character == 16 || character == 14)
        {
          /* C-P/C-N move the cursor between input lines like the TUI,
             keeping the column when the target line is long enough. */
          if (character == 16)
            {
              if (line_start == 0)
                continue;
              write_all (STDOUT_FILENO, "\033[A\r", 4);
              editor_move_line_up (&editor);
            }
          else
            {
              size_t end = editor_line_end (&editor, editor.cursor);

              if (end >= editor.length)
                continue;
              write_all (STDOUT_FILENO, "\033[B\r", 4);
              editor_move_line_down (&editor);
            }
          {
            size_t target = editor_line_start (&editor, editor.cursor);
            const char *target_prompt = prompt_display (target == 0 ? prompt : continuation_prompt);

            print_prompt (target_prompt);
            write_all (STDOUT_FILENO, editor.text + target, editor.cursor - target);
          }
          continue;
        }
      if (character == 21)
        {
          /* C-U discards the whole input like the TUI: every row is
             blanked, including its prompt, and the first-line prompt is
             reprinted at the top. The cursor may sit on any row, so the
             pass starts by stepping down to the last one. */
          size_t line = 0;
          size_t total = 1;
          size_t index;
          size_t row;

          for (index = 0; index < editor.length; index++)
            if (editor.text[index] == '\n')
              total++;
          for (index = 0; index < line_start; index++)
            if (editor.text[index] == '\n')
              line++;
          for (row = line; row + 1 < total; row++)
            write_all (STDOUT_FILENO, "\033[B", 3);
          for (row = total; row-- > 0; )
            {
              size_t span_start = 0;
              size_t span_end;
              size_t skip;

              for (skip = 0; skip < row; skip++)
                {
                  while (span_start < editor.length && editor.text[span_start] != '\n')
                    span_start++;
                  span_start++;
                }
              span_end = span_start;
              while (span_end < editor.length && editor.text[span_end] != '\n')
                span_end++;
              if (row + 1 < total)
                write_all (STDOUT_FILENO, "\033[A", 3);
              write_all (STDOUT_FILENO, "\r", 1);
              erase_cells (strlen (row == 0 ? prompt_display (prompt)
                                            : prompt_display (continuation_prompt))
                           + (size_t)editor_span_width (editor.text + span_start, span_end - span_start));
            }
          editor_clear (&editor);
          print_prompt (prompt_display (prompt));
          continue;
        }
      if (character == 11 || character == 23)
        {
          /* C-K kills to the end of the current line and C-W back to the
             start of the word before the cursor; lines below are kept. */
          if (character == 11)
            {
              size_t end = editor_line_end (&editor, editor.cursor);

              if (editor.cursor < end)
                {
                  size_t width = (size_t)editor_span_width (editor.text + editor.cursor, end - editor.cursor);

                  editor_kill_line_end (&editor);
                  erase_cells (width);
                }
              continue;
            }
          {
            size_t start = editor_word_start (&editor);

            if (start < editor.cursor)
              {
                size_t end = editor_line_end (&editor, line_start);
                size_t width = (size_t)editor_span_width (editor.text + start, editor.cursor - start);
                size_t removed = editor_delete_word (&editor);

                write_repeated ("\b", width);
                redraw_line_tail (editor.text, editor.cursor, end - removed, width);
              }
          }
          continue;
        }
      if (character == 27)
        {
          escape_state = 1;
          continue;
        }
      if ((unsigned char)character < 0x20)
        continue;
      {
        size_t end = editor_line_end (&editor, editor.cursor);

        if (editor_insert (&editor, &character, 1) < 0)
          {
            print_prompt ("\r\nerror: input is too long\n");
            tcsetattr (STDIN_FILENO, TCSANOW, &original_terminal);
            return -1;
          }
        write_all (STDOUT_FILENO, &character, 1);
        redraw_line_tail (editor.text, editor.cursor, end + 1, 0);
      }
    }
}

/* --- standalone `editor` subcommand ------------------------------- */

static volatile sig_atomic_t editor_interrupted;

static void
editor_on_signal (int signal_number)
{
  (void)signal_number;
  editor_interrupted = 1;
}

static void
editor_usage (FILE *stream)
{
  fprintf (stream, "Usage: ai editor [--help]\n"
                   "\n"
                   "Edit one multiline input and print it to standard output.\n"
                   "\n"
                   "Keys:\n"
                   "C-J                  open a new line; Enter submits\n"
                   "C-A / C-E            move to the start or end of the line\n"
                   "C-B / C-F            move one rune back or forward\n"
                   "C-P / C-N            move one line up or down\n"
                   "backspace / C-D      delete the rune before or after the cursor\n"
                   "C-K                  kill to the end of the line\n"
                   "C-W                  kill back to the start of the word\n"
                   "C-U                  clear every line of the input\n"
                   "\n"
                   "Enter prints the edited text and exits with status 0; C-C,\n"
                   "C-D on an empty input, or EOF cancels with status 130.\n");
}

/* Edit one multiline input on the terminal with the shared key bindings
   and print it to standard output. Enter submits (status 0); C-C, EOF,
   or C-D on an empty input cancels (status 130). */
int
editor_main (int argc, char **argv)
{
  static char buffer[EDITOR_STANDALONE_SIZE];
  struct sigaction action;

  if (argc > 1 && (strcmp (argv[1], "--help") == 0 || strcmp (argv[1], "-h") == 0))
    {
      editor_usage (stdout);
      return 0;
    }
  if (argc > 1)
    {
      editor_usage (stderr);
      return 2;
    }
  if (!isatty (STDIN_FILENO))
    {
      fprintf (stderr, "error: editor needs a terminal\n");
      return 1;
    }
  memset (&action, 0, sizeof (action));
  action.sa_handler = editor_on_signal;
  sigemptyset (&action.sa_mask);
  sigaction (SIGINT, &action, NULL);
  sigaction (SIGTERM, &action, NULL);
  editor_interrupted = 0;
  if (editor_read ("\n[Edit]: ", "[....]: ", buffer, sizeof (buffer)) < 0
      || editor_interrupted)
    {
      fprintf (stderr, "\nbye\n");
      return 130;
    }
  write_all (STDOUT_FILENO, buffer, strlen (buffer));
  write_all (STDOUT_FILENO, "\n", 1);
  return 0;
}
