#define _XOPEN_SOURCE 600

#include <errno.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include <ncurses.h>

#include "utf.h"

#include "version.h"

#define MAX_INPUT 8192
#define MAX_LINE 4096
#define MAX_PANE_LINES 2048
#define VERSION "1.0"
#define TOP_HEIGHT 7
#define BOTTOM_HEIGHT 5
#define MIN_TERMINAL_HEIGHT 18
#define MIN_TERMINAL_WIDTH 60

typedef enum
{
  PANE_THOUGHTS,
  PANE_COMMANDS,
  PANE_COUNT
} PaneId;

typedef enum
{
  PANE_LAYOUT_NORMAL,
  PANE_LAYOUT_EQUAL,
  PANE_LAYOUT_MAX_THOUGHTS,
  PANE_LAYOUT_MAX_COMMANDS
} PaneLayout;

typedef enum
{
  INPUT_MULTILINE,
  INPUT_SINGLE_LINE
} InputMode;

typedef struct
{
  WINDOW *border;
  WINDOW *pad;
  int line_count;
  int view_offset;
} Pane;

typedef struct
{
  char text[MAX_INPUT];
  size_t length;
  size_t cursor;
  unsigned char pending[UTFmax];
  size_t pending_length;
} InputBuffer;

static Pane panes[PANE_COUNT];
static WINDOW *input_window;
static int live_steering_active;
static int ai_request_active;
static int terminal_height;
static int terminal_width;
static volatile sig_atomic_t interrupted;
static pid_t agent_pid;
static PaneId output_mode = PANE_THOUGHTS;
static PaneId selected_pane = PANE_THOUGHTS;
static PaneLayout pane_layout = PANE_LAYOUT_EQUAL;
static InputMode input_mode = INPUT_MULTILINE;
static int full_access;

static void handle_agent_prompt (const char *line);

static void
content_pane_geometry (PaneId pane_id, int *start_y, int *height)
{
  int content_height = terminal_height - BOTTOM_HEIGHT;

  if (pane_layout == PANE_LAYOUT_EQUAL)
    {
      int thoughts_height = content_height / 2;

      if (pane_id == PANE_THOUGHTS)
        {
          *start_y = 0;
          *height = thoughts_height;
        }
      else
        {
          *start_y = thoughts_height;
          *height = content_height - thoughts_height;
        }
      return;
    }
  if (pane_layout == PANE_LAYOUT_MAX_THOUGHTS || pane_layout == PANE_LAYOUT_MAX_COMMANDS)
    {
      int maximize_thoughts = pane_layout == PANE_LAYOUT_MAX_THOUGHTS;

      *start_y = 0;
      *height = pane_id == (maximize_thoughts ? PANE_THOUGHTS : PANE_COMMANDS) ? content_height : 0;
      return;
    }
  if (pane_id == PANE_THOUGHTS)
    {
      *start_y = 0;
      *height = TOP_HEIGHT;
    }
  else
    {
      *start_y = TOP_HEIGHT;
      *height = content_height - TOP_HEIGHT;
    }
}

static void
on_signal (int signal_number)
{
  (void)signal_number;
  interrupted = 1;
  if (agent_pid > 0)
    kill (agent_pid, SIGINT);
}

static void
usage (FILE *stream)
{
  fprintf (stream, "usage: ai-tui [--full-access] [--help] [--version]\n"
                   "\n"
                   "Environment configures AI_API_URL, AI_MODEL, AI_API_KEY or\n"
                   "OPENAI_API_KEY, AI_TIMEOUT_SECONDS and AI_COMMAND_TIMEOUT_SECONDS.\n");
}

static int
parse_arguments (int argc, char **argv)
{
  int index;

  for (index = 1; index < argc; index++)
    {
      if (strcmp (argv[index], "--full-access") == 0)
        full_access = 1;
      else if (strcmp (argv[index], "--help") == 0)
        {
          usage (stdout);
          exit (0);
        }
      else if (strcmp (argv[index], "--version") == 0)
        {
          printf ("ai-tui %s %s %s\n", VERSION, BUILD_DATE, BUILD_REVISION);
          exit (0);
        }
      else
        {
          usage (stderr);
          return -1;
        }
    }
  return 0;
}

static void
destroy_layout (void)
{
  size_t index;

  for (index = 0; index < PANE_COUNT; index++)
    {
      if (panes[index].border != NULL)
        delwin (panes[index].border);
      panes[index].border = NULL;
    }
  if (input_window != NULL)
    delwin (input_window);
  input_window = NULL;
}

static void
destroy_panes (void)
{
  size_t index;

  for (index = 0; index < PANE_COUNT; index++)
    {
      if (panes[index].pad != NULL)
        delwin (panes[index].pad);
      panes[index].pad = NULL;
    }
}

static void
draw_pane_status (PaneId pane_id, WINDOW *border, int height, int width)
{
  const char *status = pane_id == PANE_THOUGHTS && live_steering_active ? " Live steering "
                       : pane_id == PANE_COMMANDS && ai_request_active   ? " AI is working "
                                                                          : "";
  int column = width - (int)strlen (status) - 2;

  if (column < 3)
    return;
  mvwprintw (border, height - 1, column, "%s", status);
}

static void
create_pane (PaneId pane_id, int height, int start_y, const char *title)
{
  Pane *pane = &panes[pane_id];
  const char *marker = pane_id == selected_pane ? " [*]" : "";

  if (height <= 0)
    return;
  pane->border = newwin (height, terminal_width, start_y, 0);
  if (pane->pad == NULL)
    {
      pane->pad = newpad (MAX_PANE_LINES, terminal_width - 2);
      pane->line_count = 0;
      pane->view_offset = 0;
      scrollok (pane->pad, TRUE);
      idlok (pane->pad, TRUE);
      wsetscrreg (pane->pad, 0, MAX_PANE_LINES - 1);
    }
  leaveok (pane->border, TRUE);
  leaveok (pane->pad, TRUE);
  box (pane->border, 0, 0);
  mvwprintw (pane->border, 0, 2, " %s%s ", title, marker);
  draw_pane_status (pane_id, pane->border, height, terminal_width);
  wnoutrefresh (pane->border);
  prefresh (pane->pad, 0, 0, start_y + 1, 1, start_y + height - 2, terminal_width - 2);
}

static int
visible_pane_start (const Pane *pane, int pane_height)
{
  int visible_start;

  if (pane->line_count > pane_height)
    visible_start = pane->line_count - pane_height - pane->view_offset;
  else
    visible_start = 0;
  return visible_start < 0 ? 0 : visible_start;
}

static void
set_pane_status (PaneId pane_id, int active)
{
  int start_y;
  int height;
  int *state = pane_id == PANE_THOUGHTS ? &live_steering_active : &ai_request_active;
  const char *title = pane_id == PANE_THOUGHTS ? "1. AI & User Thoughts" : "2. AI Command Status";

  if (*state == active)
    return;
  *state = active;
  content_pane_geometry (pane_id, &start_y, &height);
  if (panes[pane_id].border == NULL)
    return;
  redrawwin (panes[pane_id].border);
  box (panes[pane_id].border, 0, 0);
  mvwprintw (panes[pane_id].border, 0, 2, " %s%s ", title, selected_pane == pane_id ? " [*]" : "");
  draw_pane_status (pane_id, panes[pane_id].border, height, terminal_width);
  wnoutrefresh (panes[pane_id].border);
  prefresh (panes[pane_id].pad, visible_pane_start (&panes[pane_id], height - 2), 0, start_y + 1, 1,
            start_y + height - 2, terminal_width - 2);
  touchwin (input_window);
  wrefresh (input_window);
}

static void
render_pane (PaneId pane_id, int start_y, int height)
{
  Pane *pane = &panes[pane_id];
  int visible_start;

  if (height - 2 <= 0)
    return;
  visible_start = visible_pane_start (pane, height - 2);
  prefresh (pane->pad, visible_start, 0, start_y + 1, 1, start_y + height - 2, terminal_width - 2);
}

static void
refresh_content_panes (void)
{
  int thoughts_y;
  int thoughts_height;
  int commands_y;
  int commands_height;

  content_pane_geometry (PANE_THOUGHTS, &thoughts_y, &thoughts_height);
  content_pane_geometry (PANE_COMMANDS, &commands_y, &commands_height);
  render_pane (PANE_THOUGHTS, thoughts_y, thoughts_height);
  render_pane (PANE_COMMANDS, commands_y, commands_height);
  touchwin (input_window);
  wrefresh (input_window);
}

static void
redraw_layout (void)
{
  int thoughts_y;
  int thoughts_height;
  int commands_y;
  int commands_height;

  clear ();
  refresh ();
  destroy_layout ();
  content_pane_geometry (PANE_THOUGHTS, &thoughts_y, &thoughts_height);
  content_pane_geometry (PANE_COMMANDS, &commands_y, &commands_height);
  create_pane (PANE_THOUGHTS, thoughts_height, thoughts_y, "1. AI & User Thoughts");
  create_pane (PANE_COMMANDS, commands_height, commands_y, "2. AI Command Status");
  input_window = newwin (BOTTOM_HEIGHT, terminal_width, terminal_height - BOTTOM_HEIGHT, 0);
  leaveok (input_window, FALSE);
  scrollok (input_window, TRUE);
  idlok (input_window, TRUE);
  wsetscrreg (input_window, 0, BOTTOM_HEIGHT - 1);
  box (input_window, 0, 0);
  mvwprintw (input_window, 0, 2,
             input_mode == INPUT_MULTILINE ? " 3. Prompt (RET submit) " : " 3. Decision (RET submit) ");
  keypad (input_window, TRUE);
  wmove (input_window, 1, 1);
  touchwin (input_window);
  wrefresh (input_window);
}

static void
append_pane_line (PaneId pane_id, const char *text, size_t length)
{
  Pane *pane = &panes[pane_id];
  int row;

  if (pane->line_count >= MAX_PANE_LINES)
    {
      wscrl (pane->pad, 1);
      pane->line_count = MAX_PANE_LINES - 1;
    }
  row = pane->line_count;
  mvwaddnstr (pane->pad, row, 0, text, (int)length);
  pane->line_count++;
}

static void
write_wrapped_line (PaneId pane_id, const char *text)
{
  size_t width = (size_t)(terminal_width - 2);
  size_t start = 0;
  size_t used_width = 0;
  size_t rune_start = 0;

  while (text[start] != 0)
    {
      Rune rune;
      int rune_length = chartorune (&rune, (char *)text + start);
      int rune_width = wcwidth ((wchar_t)rune);

      if (rune_width < 0)
        rune_width = 1;
      if (used_width + (size_t)rune_width > width && start != rune_start)
        {
          append_pane_line (pane_id, text + rune_start, start - rune_start);
          rune_start = start;
          used_width = 0;
        }
      used_width += (size_t)rune_width;
      start += (size_t)rune_length;
      if (rune_length <= 0)
        break;
    }
  append_pane_line (pane_id, text + rune_start, strlen (text + rune_start));
}

static void
write_multiline (PaneId pane_id, const char *text)
{
  size_t start = 0;
  size_t index;

  for (index = 0;; index++)
    {
      if (text[index] == '\n' || text[index] == 0)
        {
          append_pane_line (pane_id, text + start, index - start);
          if (text[index] == 0)
            break;
          start = index + 1;
        }
    }
  panes[pane_id].view_offset = 0;
}

static void
write_line (PaneId pane_id, const char *text)
{
  if (text[0] == 0)
    append_pane_line (pane_id, "", 0);
  else
    write_wrapped_line (pane_id, text);
  panes[pane_id].view_offset = 0;
}

static int
is_agent_prompt (const char *line)
{
  return strcmp (line, "[You]: ") == 0 || strcmp (line, "[...]: ") == 0 || strcmp (line, "[steer]: ") == 0
         || strcmp (line, "[.....]: ") == 0
         || strcmp (line, "[a]llow, [e]dit, [s]teer without running, [n]o, [q]uit: ") == 0
         || strcmp (line, "[edited command]: ") == 0
         || strcmp (line, "[s]teer before next AI request, Enter to continue, q quit: ") == 0;
}

static const char *
agent_prompt_hint (const char *line)
{
  if (strcmp (line, "[a]llow, [e]dit, [s]teer without running, [n]o, [q]uit: ") == 0)
    return "Command decision: a allow, e edit, s steer, n no, q quit";
  if (strcmp (line, "[edited command]: ") == 0)
    return "Edited command: enter the replacement command";
  if (strcmp (line, "[s]teer before next AI request, Enter to continue, q quit: ") == 0)
    return "Next request: s steer, Enter continue, q quit (after command output)";
  if (strcmp (line, "[live steering active]: type one line per direction") == 0)
    return "Live steering: type one line per direction";
  if (strcmp (line, "Steering ends with an empty line.") == 0)
    return "Steering: C-J newline, Enter submit";
  return NULL;
}

static void
display_agent_line (const char *line)
{
  const char *hint;

  if (strncmp (line, "[AI Response]", 13) == 0)
    output_mode = PANE_THOUGHTS;
  else if (strncmp (line, "[AI requests command]", 21) == 0 || strncmp (line, "[command]", 9) == 0
           || strncmp (line, "[command output]", 16) == 0
           || strncmp (line, "Command output:", 15) == 0
           || strncmp (line, "[command timed out after", 24) == 0
           || strncmp (line, "[command completed]", 19) == 0)
    output_mode = PANE_COMMANDS;
  else if (strncmp (line, "[You]", 5) == 0 || strncmp (line, "[live steering", 14) == 0
           || strncmp (line, "[a]llow", 7) == 0 || strncmp (line, "[edited command]", 16) == 0
           || strncmp (line, "[steer]", 7) == 0 || strncmp (line, "[s]teer", 7) == 0)
    output_mode = PANE_THOUGHTS;
  hint = agent_prompt_hint (line);
  if (strcmp (line, "[AI request active]") == 0)
    {
      set_pane_status (PANE_COMMANDS, 1);
      return;
    }
  if (strcmp (line, "[AI request inactive]") == 0)
    {
      set_pane_status (PANE_COMMANDS, 0);
      return;
    }
  if (strcmp (line, "[live steering active]") == 0)
    {
      set_pane_status (PANE_THOUGHTS, 1);
      return;
    }
  if (strcmp (line, "[live steering inactive]") == 0)
    {
      set_pane_status (PANE_THOUGHTS, 0);
      return;
    }
  if (is_agent_prompt (line) || hint != NULL)
    {
      if (hint != NULL)
        write_line (PANE_THOUGHTS, hint);
      handle_agent_prompt (line);
      return;
    }
  write_line (output_mode, line);
  handle_agent_prompt (line);
}

static int
measure_input_width (const char *buffer, size_t length)
{
  size_t index = 0;
  int width = 0;

  while (index < length)
    {
      Rune rune;
      int rune_length = chartorune (&rune, (char *)buffer + index);
      int rune_width = wcwidth ((wchar_t)rune);

      if (rune_width < 0)
        rune_width = 1;
      width += rune_width;
      index += (size_t)(rune_length > 0 ? rune_length : 1);
    }
  return width;
}

static void
draw_input (const InputBuffer *input)
{
  size_t line_start = 0;
  size_t cursor_line_start = 0;
  size_t visible_lines = 0;
  size_t first_visible_line = 0;
  size_t logical_line = 0;
  size_t index;
  size_t cursor_row = 0;
  size_t visible_height = (size_t)(BOTTOM_HEIGHT - 2);

  for (index = 0; index <= input->length; index++)
    {
      if (index < input->cursor && input->text[index] == '\n')
        {
          cursor_line_start = index + 1;
          cursor_row++;
        }
    }
  if (cursor_row >= visible_height)
    first_visible_line = cursor_row - visible_height + 1;
  for (index = 0; index < input->length && logical_line < first_visible_line; index++)
    if (input->text[index] == '\n')
      {
        line_start = index + 1;
        logical_line++;
      }
  werase (input_window);
  box (input_window, 0, 0);
  mvwprintw (input_window, 0, 2,
             input_mode == INPUT_MULTILINE ? " 3. Prompt (C-J newline, RET submit) " : " 3. Decision (RET submit) ");
  for (index = line_start; index <= input->length && visible_lines < visible_height;)
    {
      size_t end = index;
      int column = 0;

      while (end < input->length && input->text[end] != '\n')
        {
          Rune rune;
          int rune_length = chartorune (&rune, (char *)input->text + end);
          int rune_width = wcwidth ((wchar_t)rune);

          if (rune_width < 0)
            rune_width = 1;
          if (column + rune_width <= terminal_width - 3)
            mvwaddnstr (input_window, (int)visible_lines + 1, column + 1, input->text + end, (size_t)rune_length);
          column += rune_width;
          end += (size_t)(rune_length > 0 ? rune_length : 1);
        }
      if (visible_lines + first_visible_line == cursor_row)
        {
          int cursor_column = measure_input_width (input->text + cursor_line_start, input->cursor - cursor_line_start);

          if (cursor_column > terminal_width - 3)
            cursor_column = terminal_width - 3;
          wmove (input_window, (int)visible_lines + 1, cursor_column + 1);
        }
      visible_lines++;
      if (end < input->length)
        end++;
      index = end;
    }
  if (input->length == 0)
    wmove (input_window, 1, 1);
  wrefresh (input_window);
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
submit_input (int agent_stdin, const char *line)
{
  size_t length = strlen (line);

  if (length == 0 && input_mode == INPUT_MULTILINE)
    return;
  if (write_all (agent_stdin, line, length) < 0
      || write_all (agent_stdin, input_mode == INPUT_MULTILINE ? "\n\n" : "\n", input_mode == INPUT_MULTILINE ? 2 : 1)
             < 0)
    {
      write_line (PANE_THOUGHTS, "[error]: unable to send prompt to agent");
      return;
    }
  if (length != 0)
    {
      write_line (PANE_THOUGHTS, "[You]:");
      write_multiline (PANE_THOUGHTS, line);
    }
}

static void
scroll_pane (PaneId pane_id, int amount)
{
  Pane *pane = &panes[pane_id];
  int start_y;
  int height;

  if (pane->border == NULL || pane->pad == NULL)
    return;

  pane->view_offset += amount;
  if (pane->view_offset < 0)
    pane->view_offset = 0;
  if (pane->view_offset > pane->line_count)
    pane->view_offset = pane->line_count;
  content_pane_geometry (pane_id, &start_y, &height);
  render_pane (pane_id, start_y, height);
  mvwprintw (panes[pane_id].border, 0, terminal_width - 12, " %5d ", pane->view_offset);
  wnoutrefresh (panes[pane_id].border);
  wrefresh (input_window);
}

static void
start_agent (const char *agent_path, int *agent_stdin, int *agent_stdout)
{
  int input_fds[2];
  int output_fds[2];
  pid_t child;

  if (pipe (input_fds) < 0)
    {
      endwin ();
      fprintf (stderr, "error: create agent input pipe: %s\n", strerror (errno));
      exit (1);
    }
  if (pipe (output_fds) < 0)
    {
      close (input_fds[0]);
      close (input_fds[1]);
      endwin ();
      fprintf (stderr, "error: create agent output pipe: %s\n", strerror (errno));
      exit (1);
    }
  child = fork ();
  if (child < 0)
    {
      close (input_fds[0]);
      close (input_fds[1]);
      close (output_fds[0]);
      close (output_fds[1]);
      endwin ();
      fprintf (stderr, "error: fork agent: %s\n", strerror (errno));
      exit (1);
    }
  if (child == 0)
    {
      dup2 (input_fds[0], STDIN_FILENO);
      dup2 (output_fds[1], STDOUT_FILENO);
      dup2 (output_fds[1], STDERR_FILENO);
      close (input_fds[0]);
      close (input_fds[1]);
      close (output_fds[0]);
      close (output_fds[1]);
      if (full_access)
        execl (agent_path, "ai", "--ui", "--full-access", (char *)NULL);
      else
        execl (agent_path, "ai", "--ui", (char *)NULL);
      _exit (127);
    }
  close (input_fds[0]);
  close (output_fds[1]);
  *agent_stdin = input_fds[1];
  *agent_stdout = output_fds[0];
  agent_pid = child;
}

static void
handle_agent_prompt (const char *line)
{
  int multiline;

  if (strcmp (line, "[You]: ") == 0 || strcmp (line, "[...]: ") == 0 || strcmp (line, "[steer]: ") == 0
      || strcmp (line, "[.....]: ") == 0)
    multiline = 1;
  else if (strcmp (line, "[a]llow, [e]dit, [s]teer without running, [n]o, [q]uit: ") == 0
           || strcmp (line, "[edited command]: ") == 0
           || strcmp (line, "[s]teer before next AI request, Enter to continue, q quit: ") == 0)
    multiline = 0;
  else
    return;
  input_mode = multiline ? INPUT_MULTILINE : INPUT_SINGLE_LINE;
}

static void
read_agent_output (int agent_stdout, int *agent_exited)
{
  static char pending[MAX_LINE];
  static size_t pending_length;
  static time_t pending_time;
  char chunk[1024];
  struct pollfd descriptor = { agent_stdout, POLLIN, 0 };
  ssize_t received;
  time_t now;

  if (poll (&descriptor, 1, 0) < 0 && errno != EINTR)
    return;
  if (descriptor.revents & (POLLHUP | POLLERR))
    *agent_exited = 1;
  if (!(descriptor.revents & POLLIN))
    {
      if (pending_length != 0 && time (&now) != (time_t)-1 && now - pending_time >= 1)
        {
          pending[pending_length] = 0;
          display_agent_line (pending);
          pending_length = 0;
          refresh_content_panes ();
        }
      return;
    }
  received = read (agent_stdout, chunk, sizeof (chunk));
  if (received == 0)
    {
      *agent_exited = 1;
      return;
    }
  if (received < 0)
    {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
        return;
      *agent_exited = 1;
      return;
    }
  if (pending_length == 0)
    pending_time = time (NULL);
  for (size_t index = 0; index < (size_t)received; index++)
    {
      if (pending_length == sizeof (pending) - 1 || chunk[index] == '\n' || chunk[index] == '\r')
        {
          pending[pending_length] = 0;
          if (pending_length != 0)
            display_agent_line (pending);
          pending_length = 0;
        }
      else
        pending[pending_length++] = chunk[index];
    }
  refresh_content_panes ();
}

static size_t
previous_rune_start (const InputBuffer *input, size_t cursor)
{
  size_t index = 0;
  size_t last_start = 0;

  while (index < cursor)
    {
      Rune rune;
      int rune_length = chartorune (&rune, (char *)input->text + index);

      last_start = index;
      index += (size_t)(rune_length > 0 ? rune_length : 1);
    }
  return last_start;
}

static size_t
input_line_start (const InputBuffer *input, size_t position)
{
  size_t start = position;

  while (start > 0 && input->text[start - 1] != '\n')
    start--;
  return start;
}

static size_t
input_line_end (const InputBuffer *input, size_t position)
{
  size_t end = position;

  while (end < input->length && input->text[end] != '\n')
    end++;
  return end;
}

static void
move_cursor_backward (InputBuffer *input)
{
  if (input->cursor != 0)
    input->cursor = previous_rune_start (input, input->cursor);
}

static void
move_cursor_forward (InputBuffer *input)
{
  if (input->cursor < input->length)
    {
      Rune rune;
      int rune_length = chartorune (&rune, input->text + input->cursor);

      input->cursor += (size_t)(rune_length > 0 ? rune_length : 1);
    }
}

static void
move_cursor_up (InputBuffer *input)
{
  size_t start;
  size_t end;
  size_t target;

  if (input_line_start (input, input->cursor) == 0)
    return;
  end = input_line_start (input, input->cursor) - 1;
  start = input_line_start (input, end);
  target = input->cursor - end - 1;
  if (target > end - start)
    target = end - start;
  input->cursor = start + target;
}

static void
move_cursor_down (InputBuffer *input)
{
  size_t end = input_line_end (input, input->cursor);
  size_t next_end;
  size_t target;

  if (end >= input->length)
    return;
  next_end = input_line_end (input, end + 1);
  target = input->cursor - input_line_start (input, input->cursor);
  if (target > next_end - end - 1)
    target = next_end - end - 1;
  input->cursor = end + 1 + target;
}

static void
insert_input_rune (InputBuffer *input, const unsigned char *bytes, size_t length)
{
  if (input->length + length >= sizeof (input->text))
    {
      beep ();
      return;
    }
  memmove (input->text + input->cursor + length, input->text + input->cursor, input->length - input->cursor);
  memcpy (input->text + input->cursor, bytes, length);
  input->cursor += length;
  input->length += length;
  input->text[input->length] = 0;
}

static void
delete_input_backward (InputBuffer *input)
{
  size_t start;

  if (input->pending_length != 0)
    {
      input->pending_length = 0;
      return;
    }
  if (input->cursor == 0)
    return;
  start = previous_rune_start (input, input->cursor);
  memmove (input->text + start, input->text + input->cursor, input->length - input->cursor);
  input->length -= input->cursor - start;
  input->cursor = start;
  input->text[input->length] = 0;
}

static void
delete_input_forward (InputBuffer *input)
{
  size_t next;
  size_t end;
  InputBuffer temporary;

  if (input->cursor >= input->length)
    return;
  next = input->cursor;
  temporary = *input;
  move_cursor_forward (&temporary);
  end = temporary.cursor;
  memmove (input->text + next, input->text + end, input->length - end);
  input->length -= end - next;
  input->cursor = next;
  input->text[input->length] = 0;
}

static const char *
agent_executable_path (char *buffer, size_t capacity)
{
  ssize_t length = readlink ("/proc/self/exe", buffer, capacity - 1);
  char *slash;

  if (length < 0)
    return NULL;
  buffer[length] = 0;
  slash = strrchr (buffer, '/');
  if (slash == NULL)
    return NULL;
  strcpy (slash + 1, "ai");
  return buffer;
}

int
tui_main (int argc, char **argv)
{
  char executable_path[4096];
  InputBuffer input = { { 0 }, 0, 0, { 0 }, 0 };
  struct sigaction action;
  const char *agent_path;
  int agent_stdin = -1;
  int agent_stdout = -1;
  int agent_exited = 0;
  int status;

  if (parse_arguments (argc, argv) < 0)
    return 2;
  agent_path = agent_executable_path (executable_path, sizeof (executable_path));
  if (agent_path == NULL)
    {
      fprintf (stderr, "error: unable to locate the ai executable\n");
      return 1;
    }
  memset (&action, 0, sizeof (action));
  action.sa_handler = on_signal;
  sigemptyset (&action.sa_mask);
  sigaction (SIGINT, &action, NULL);
  sigaction (SIGTERM, &action, NULL);
  setlocale (LC_ALL, "");
  initscr ();
  nonl ();
  cbreak ();
  noecho ();
  raw ();
  getmaxyx (stdscr, terminal_height, terminal_width);
  if (terminal_height < MIN_TERMINAL_HEIGHT || terminal_width < MIN_TERMINAL_WIDTH)
    {
      endwin ();
      fprintf (stderr, "error: terminal must be at least %dx%d\n", MIN_TERMINAL_WIDTH, MIN_TERMINAL_HEIGHT);
      return 1;
    }
  redraw_layout ();
  start_agent (agent_path, &agent_stdin, &agent_stdout);
  wtimeout (input_window, 100);
  write_line (PANE_THOUGHTS, "F1 quit; F2/F3 maximize pane 1/2; Tab pane; PgUp/PgDn scroll; "
                             "C-A/E/B/F/D/K/U edit; "
                             "C-J newline; Enter submit; Ctrl+C cancel");
  refresh_content_panes ();
  draw_input (&input);
  while (!interrupted && !agent_exited)
    {
      int key = wgetch (input_window);

      read_agent_output (agent_stdout, &agent_exited);
      if (key == ERR)
        continue;
      if (key == KEY_F (1) || key == 17)
        break;
      if (key == KEY_RESIZE)
        {
          getmaxyx (stdscr, terminal_height, terminal_width);
          if (terminal_height < MIN_TERMINAL_HEIGHT || terminal_width < MIN_TERMINAL_WIDTH)
            break;
          redraw_layout ();
          draw_input (&input);
          continue;
        }
      if (key == '\t')
        {
          selected_pane = selected_pane == PANE_THOUGHTS ? PANE_COMMANDS : PANE_THOUGHTS;
          pane_layout = PANE_LAYOUT_EQUAL;
          redraw_layout ();
          draw_input (&input);
          continue;
        }
      if (key == KEY_F (2))
        {
          selected_pane = PANE_THOUGHTS;
          pane_layout = pane_layout == PANE_LAYOUT_MAX_THOUGHTS ? PANE_LAYOUT_EQUAL : PANE_LAYOUT_MAX_THOUGHTS;
          redraw_layout ();
          draw_input (&input);
          continue;
        }
      if (key == KEY_F (3))
        {
          selected_pane = PANE_COMMANDS;
          pane_layout = pane_layout == PANE_LAYOUT_MAX_COMMANDS ? PANE_LAYOUT_EQUAL : PANE_LAYOUT_MAX_COMMANDS;
          redraw_layout ();
          draw_input (&input);
          continue;
        }
      if (key == KEY_PPAGE)
        {
          scroll_pane (selected_pane, 3);
          continue;
        }
      if (key == KEY_NPAGE)
        {
          scroll_pane (selected_pane, -3);
          continue;
        }
      if (key == 1 || key == KEY_HOME)
        input.cursor = input_line_start (&input, input.cursor);
      else if (key == 5 || key == KEY_END)
        input.cursor = input_line_end (&input, input.cursor);
      else if (key == 2 || key == KEY_LEFT)
        move_cursor_backward (&input);
      else if (key == 6 || key == KEY_RIGHT)
        move_cursor_forward (&input);
      else if (key == 16 || key == KEY_UP)
        move_cursor_up (&input);
      else if (key == 14 || key == KEY_DOWN)
        move_cursor_down (&input);
      else if (key == KEY_BACKSPACE || key == 127 || key == 8)
        delete_input_backward (&input);
      else if (key == 4)
        delete_input_forward (&input);
      else if (key == 21)
        {
          input.length = 0;
          input.cursor = 0;
          input.pending_length = 0;
          input.text[input.length] = 0;
        }
      else if (key == 11)
        {
          input.length = input_line_end (&input, input.cursor);
          input.text[input.length] = 0;
        }
      else if (key == 10 && input_mode == INPUT_MULTILINE)
        insert_input_rune (&input, (const unsigned char *)"\n", 1);
      else if (key == '\n' || key == KEY_ENTER || key == 13)
        {
          input.text[input.length] = 0;
          submit_input (agent_stdin, input.text);
          input.length = 0;
          input.cursor = 0;
          input.pending_length = 0;
        }
      else if (key >= 32 && key != 127)
        {
          if (input.pending_length >= sizeof (input.pending))
            {
              beep ();
              input.pending_length = 0;
              draw_input (&input);
              continue;
            }
          input.pending[input.pending_length++] = (unsigned char)key;
          if (fullrune ((char *)input.pending, (int)input.pending_length))
            {
              insert_input_rune (&input, input.pending, input.pending_length);
              input.pending_length = 0;
            }
        }
      draw_input (&input);
    }
  if (agent_pid > 0)
    {
      kill (agent_pid, SIGTERM);
      waitpid (agent_pid, &status, 0);
    }
  if (agent_stdin >= 0)
    close (agent_stdin);
  if (agent_stdout >= 0)
    close (agent_stdout);
  destroy_layout ();
  destroy_panes ();
  endwin ();
  return 0;
}
