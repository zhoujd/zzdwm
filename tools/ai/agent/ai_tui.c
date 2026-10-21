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

#include <ncursesw/ncurses.h>

#include "utf.h"

#define MAX_INPUT 8192
#define MAX_LINE 4096
#define MAX_PANE_LINES 2048
#define TOP_HEIGHT 7
#define BOTTOM_HEIGHT 5
#define MIN_TERMINAL_HEIGHT 18
#define MIN_TERMINAL_WIDTH 60

typedef enum {
  PANE_LOGS,
  PANE_RESPONSE,
  PANE_COUNT
} PaneId;

typedef enum {
  INPUT_MULTILINE,
  INPUT_SINGLE_LINE
} InputMode;

typedef struct {
  WINDOW *border;
  WINDOW *pad;
  int line_count;
  int view_offset;
} Pane;

static Pane panes[PANE_COUNT];
static WINDOW *input_window;
static int terminal_height;
static int terminal_width;
static volatile sig_atomic_t interrupted;
static pid_t agent_pid;
static int output_mode_response;
static PaneId selected_pane = PANE_LOGS;
static InputMode input_mode = INPUT_MULTILINE;

static void handle_agent_prompt(const char *line);

static void on_signal(int signal_number)
{
  (void) signal_number;
  interrupted = 1;
  if (agent_pid > 0)
    kill(agent_pid, SIGINT);
}

static void destroy_layout(void)
{
  size_t index;

  for (index = 0; index < PANE_COUNT; index++)
    {
      if (panes[index].border != NULL)
        delwin(panes[index].border);
      if (panes[index].pad != NULL)
        delwin(panes[index].pad);
      panes[index].border = NULL;
      panes[index].pad = NULL;
    }
  if (input_window != NULL)
    delwin(input_window);
  input_window = NULL;
}

static void create_pane(PaneId pane_id, int height, int start_y,
                        const char *title)
{
  Pane *pane = &panes[pane_id];
  const char *marker = pane_id == selected_pane ? " [*]" : "";

  pane->border = newwin(height, terminal_width, start_y, 0);
  pane->pad = newpad(MAX_PANE_LINES, terminal_width - 2);
  pane->line_count = 0;
  pane->view_offset = 0;
  scrollok(pane->pad, TRUE);
  idlok(pane->pad, TRUE);
  wsetscrreg(pane->pad, 0, MAX_PANE_LINES - 1);
  box(pane->border, 0, 0);
  mvwprintw(pane->border, 0, 2, " %s%s ", title, marker);
  wnoutrefresh(pane->border);
  prefresh(pane->pad, 0, 0, start_y + 1, 1,
           start_y + height - 2, terminal_width - 2);
}

static void render_pane(PaneId pane_id, int start_y, int height)
{
  Pane *pane = &panes[pane_id];
  int pane_height = height - 2;
  int visible_start;

  if (pane_height <= 0)
    return;
  if (pane->line_count > pane_height)
    visible_start = pane->line_count - pane_height - pane->view_offset;
  else
    visible_start = 0;
  if (visible_start < 0)
    visible_start = 0;
  prefresh(pane->pad, visible_start, 0, start_y + 1, 1,
           start_y + height - 2, terminal_width - 2);
}

static void refresh_content_panes(void)
{
  int response_height = terminal_height - TOP_HEIGHT - BOTTOM_HEIGHT;

  render_pane(PANE_LOGS, 0, TOP_HEIGHT);
  render_pane(PANE_RESPONSE, TOP_HEIGHT, response_height);
}

static void redraw_layout(void)
{
  int response_height;

  clear();
  refresh();
  destroy_layout();
  response_height = terminal_height - TOP_HEIGHT - BOTTOM_HEIGHT;
  create_pane(PANE_LOGS, TOP_HEIGHT, 0,
              "1. Agent Thoughts & System Status");
  create_pane(PANE_RESPONSE, response_height, TOP_HEIGHT,
              "2. AI Response");
  input_window = newwin(BOTTOM_HEIGHT, terminal_width,
                        TOP_HEIGHT + response_height, 0);
  scrollok(input_window, TRUE);
  idlok(input_window, TRUE);
  wsetscrreg(input_window, 0, BOTTOM_HEIGHT - 1);
  box(input_window, 0, 0);
  mvwprintw(input_window, 0, 2, " 3. Keyboard Input ");
  keypad(input_window, TRUE);
  wrefresh(input_window);
}

static void append_pane_line(PaneId pane_id, const char *text, size_t length)
{
  Pane *pane = &panes[pane_id];
  int row;

  if (pane->line_count >= MAX_PANE_LINES)
    {
      wscrl(pane->pad, 1);
      pane->line_count = MAX_PANE_LINES - 1;
    }
  row = pane->line_count;
  mvwaddnstr(pane->pad, row, 0, text, (int) length);
  pane->line_count++;
}

static void write_wrapped_line(PaneId pane_id, const char *text)
{
  size_t width = (size_t) (terminal_width - 2);
  size_t start = 0;
  size_t used_width = 0;
  size_t rune_start = 0;

  while (text[start] != 0)
    {
      Rune rune;
      int rune_length = chartorune(&rune, (char *) text + start);
      int rune_width = wcwidth((wchar_t) rune);

      if (rune_width < 0)
        rune_width = 1;
      if (used_width + (size_t) rune_width > width && start != rune_start)
        {
          append_pane_line(pane_id, text + rune_start, start - rune_start);
          rune_start = start;
          used_width = 0;
        }
      used_width += (size_t) rune_width;
      start += (size_t) rune_length;
      if (rune_length <= 0)
        break;
    }
  append_pane_line(pane_id, text + rune_start, strlen(text + rune_start));
}

static void write_line(PaneId pane_id, const char *text)
{
  if (text[0] == 0)
    append_pane_line(pane_id, "", 0);
  else
    write_wrapped_line(pane_id, text);
  panes[pane_id].view_offset = 0;
}

static void display_agent_line(const char *line)
{
  if (strncmp(line, "[AI Response]", 13) == 0)
    output_mode_response = 1;
  else if (strncmp(line, "[You]", 5) == 0
           || strncmp(line, "[AI requests command]", 21) == 0
           || strncmp(line, "[live steering", 14) == 0
           || strncmp(line, "[a]llow", 7) == 0
           || strncmp(line, "[edited command]", 16) == 0
           || strncmp(line, "[steer]", 7) == 0
           || strncmp(line, "[s]teer", 7) == 0)
    output_mode_response = 0;
  write_line(output_mode_response ? PANE_RESPONSE : PANE_LOGS, line);
  handle_agent_prompt(line);
}

static int measure_input_width(const char *buffer, size_t length)
{
  size_t index = 0;
  int width = 0;

  while (index < length)
    {
      Rune rune;
      int rune_length = chartorune(&rune, (char *) buffer + index);
      int rune_width = wcwidth((wchar_t) rune);

      if (rune_width < 0)
        rune_width = 1;
      width += rune_width;
      index += (size_t) (rune_length > 0 ? rune_length : 1);
    }
  return width;
}

static void draw_input(const char *buffer, size_t length)
{
  int pane_width = terminal_width - 3;
  int input_width = measure_input_width(buffer, length);
  size_t start = 0;

  while (input_width > pane_width)
    {
      Rune rune;
      int rune_length = chartorune(&rune, (char *) buffer + start);
      int rune_width = wcwidth((wchar_t) rune);

      if (rune_width < 0)
        rune_width = 1;
      input_width -= rune_width;
      start += (size_t) (rune_length > 0 ? rune_length : 1);
    }
  werase(input_window);
  box(input_window, 0, 0);
  mvwprintw(input_window, 0, 2, " 3. Keyboard Input ");
  mvwaddstr(input_window, 1, 1, buffer + start);
  wmove(input_window, 1, input_width + 1);
  wrefresh(input_window);
}

static int write_all(int file_descriptor, const char *data, size_t length)
{
  while (length > 0)
    {
      ssize_t written = write(file_descriptor, data, length);

      if (written < 0 && errno == EINTR)
        continue;
      if (written <= 0)
        return -1;
      data += written;
      length -= (size_t) written;
    }
  return 0;
}

static void submit_input(int agent_stdin, const char *line)
{
  size_t length = strlen(line);

  if (length == 0)
    return;
  if (write_all(agent_stdin, line, length) < 0
      || write_all(agent_stdin,
                   input_mode == INPUT_MULTILINE ? "\n\n" : "\n",
                   input_mode == INPUT_MULTILINE ? 2 : 1) < 0)
    {
      write_line(PANE_LOGS, "[error]: unable to send prompt to agent");
      return;
    }
  write_line(PANE_LOGS, "[You]:");
  write_line(PANE_LOGS, line);
}

static void scroll_pane(PaneId pane_id, int amount)
{
  Pane *pane = &panes[pane_id];
  int response_height = terminal_height - TOP_HEIGHT - BOTTOM_HEIGHT;

  pane->view_offset += amount;
  if (pane->view_offset < 0)
    pane->view_offset = 0;
  if (pane->view_offset > pane->line_count)
    pane->view_offset = pane->line_count;
  if (pane_id == PANE_LOGS)
    render_pane(PANE_LOGS, 0, TOP_HEIGHT);
  else
    render_pane(PANE_RESPONSE, TOP_HEIGHT, response_height);
  mvwprintw(panes[pane_id].border, 0, terminal_width - 12, " %5d ",
            pane->view_offset);
  wrefresh(panes[pane_id].border);
}

static void start_agent(const char *agent_path, int *agent_stdin,
                        int *agent_stdout)
{
  int input_fds[2];
  int output_fds[2];
  pid_t child;

  if (pipe(input_fds) < 0)
    {
      endwin();
      fprintf(stderr, "error: create agent input pipe: %s\n", strerror(errno));
      exit(1);
    }
  if (pipe(output_fds) < 0)
    {
      close(input_fds[0]);
      close(input_fds[1]);
      endwin();
      fprintf(stderr, "error: create agent output pipe: %s\n", strerror(errno));
      exit(1);
    }
  child = fork();
  if (child < 0)
    {
      close(input_fds[0]);
      close(input_fds[1]);
      close(output_fds[0]);
      close(output_fds[1]);
      endwin();
      fprintf(stderr, "error: fork agent: %s\n", strerror(errno));
      exit(1);
    }
  if (child == 0)
    {
      dup2(input_fds[0], STDIN_FILENO);
      dup2(output_fds[1], STDOUT_FILENO);
      dup2(output_fds[1], STDERR_FILENO);
      close(input_fds[0]);
      close(input_fds[1]);
      close(output_fds[0]);
      close(output_fds[1]);
      execl(agent_path, "ai", "--ui", (char *) NULL);
      _exit(127);
    }
  close(input_fds[0]);
  close(output_fds[1]);
  *agent_stdin = input_fds[1];
  *agent_stdout = output_fds[0];
  agent_pid = child;
}

static void handle_agent_prompt(const char *line)
{
  int multiline;

  if (strcmp(line, "[You]: ") == 0
      || strcmp(line, "[...]: ") == 0
      || strcmp(line, "[steer]: ") == 0
      || strcmp(line, "[.....]: ") == 0)
    multiline = 1;
  else if (strcmp(line,
             "[a]llow, [e]dit, [s]teer without running, [n]o, [q]uit: ") == 0
           || strcmp(line, "[edited command]: ") == 0
           || strcmp(line,
             "[s]teer before next AI request, Enter to continue, q quit: ") == 0)
    multiline = 0;
  else
    return;
  input_mode = multiline ? INPUT_MULTILINE : INPUT_SINGLE_LINE;
  write_line(PANE_LOGS,
             multiline
             ? "End input with an empty line (Enter twice)."
             : "Send a choice with Enter; empty Enter accepts the default.");
}

static void read_agent_output(int agent_stdout, int *agent_exited)
{
  static char pending[MAX_LINE];
  static size_t pending_length;
  static time_t pending_time;
  char chunk[1024];
  struct pollfd descriptor = {agent_stdout, POLLIN, 0};
  ssize_t received;
  time_t now;

  if (poll(&descriptor, 1, 0) < 0 && errno != EINTR)
    return;
  if (descriptor.revents & (POLLHUP | POLLERR))
    *agent_exited = 1;
  if (!(descriptor.revents & POLLIN))
    {
      if (pending_length != 0 && time(&now) != (time_t) -1
          && now - pending_time >= 1)
        {
          pending[pending_length] = 0;
          display_agent_line(pending);
          pending_length = 0;
          refresh_content_panes();
        }
      return;
    }
  received = read(agent_stdout, chunk, sizeof(chunk));
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
    pending_time = time(NULL);
  for (size_t index = 0; index < (size_t) received; index++)
    {
      if (pending_length == sizeof(pending) - 1
          || chunk[index] == '\n' || chunk[index] == '\r')
        {
          pending[pending_length] = 0;
          if (pending_length != 0)
            display_agent_line(pending);
          pending_length = 0;
        }
      else
        pending[pending_length++] = chunk[index];
    }
  refresh_content_panes();
}

static void remove_last_input_rune(char *buffer, size_t *length)
{
  size_t index = 0;
  size_t last_start = 0;

  while (index < *length)
    {
      Rune rune;
      int rune_length = chartorune(&rune, buffer + index);

      last_start = index;
      index += (size_t) (rune_length > 0 ? rune_length : 1);
    }
  *length = last_start;
}

static const char *agent_executable_path(char *buffer, size_t capacity)
{
  ssize_t length = readlink("/proc/self/exe", buffer, capacity - 1);
  char *slash;

  if (length < 0)
    return NULL;
  buffer[length] = 0;
  slash = strrchr(buffer, '/');
  if (slash == NULL)
    return NULL;
  strcpy(slash + 1, "ai");
  return buffer;
}

int main(void)
{
  char executable_path[4096];
  char input_buffer[MAX_INPUT];
  unsigned char input_pending[UTFmax];
  size_t input_length = 0;
  size_t pending_length = 0;
  struct sigaction action;
  const char *agent_path;
  int agent_stdin = -1;
  int agent_stdout = -1;
  int agent_exited = 0;
  int status;

  agent_path = agent_executable_path(executable_path, sizeof(executable_path));
  if (agent_path == NULL)
    {
      fprintf(stderr, "error: unable to locate the ai executable\n");
      return 1;
    }
  memset(&action, 0, sizeof(action));
  action.sa_handler = on_signal;
  sigemptyset(&action.sa_mask);
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  setlocale(LC_ALL, "");
  initscr();
  cbreak();
  noecho();
  getmaxyx(stdscr, terminal_height, terminal_width);
  if (terminal_height < MIN_TERMINAL_HEIGHT
      || terminal_width < MIN_TERMINAL_WIDTH)
    {
      endwin();
      fprintf(stderr, "error: terminal must be at least %dx%d\n",
              MIN_TERMINAL_WIDTH, MIN_TERMINAL_HEIGHT);
      return 1;
    }
  redraw_layout();
  start_agent(agent_path, &agent_stdin, &agent_stdout);
  wtimeout(input_window, 100);
  write_line(PANE_LOGS,
             "F1 quit; Tab select pane; PgUp/PgDn scroll; Ctrl+C cancel");
  refresh_content_panes();
  while (!interrupted && !agent_exited)
    {
      int key = wgetch(input_window);

      read_agent_output(agent_stdout, &agent_exited);
      if (key == ERR)
        continue;
      if (key == KEY_F(1) || key == 17)
        break;
      if (key == KEY_RESIZE)
        {
          getmaxyx(stdscr, terminal_height, terminal_width);
          if (terminal_height < MIN_TERMINAL_HEIGHT
              || terminal_width < MIN_TERMINAL_WIDTH)
            break;
          redraw_layout();
          draw_input(input_buffer, input_length);
          continue;
        }
      if (key == '\t')
        {
          selected_pane = selected_pane == PANE_LOGS
                          ? PANE_RESPONSE : PANE_LOGS;
          redraw_layout();
          draw_input(input_buffer, input_length);
          continue;
        }
      if (key == KEY_PPAGE)
        {
          scroll_pane(selected_pane, 3);
          continue;
        }
      if (key == KEY_NPAGE)
        {
          scroll_pane(selected_pane, -3);
          continue;
        }
      if ((key == KEY_BACKSPACE || key == 127 || key == 8)
          && (input_length > 0 || pending_length > 0))
        {
          if (pending_length != 0)
            pending_length = 0;
          else
            remove_last_input_rune(input_buffer, &input_length);
        }
      else if (key == '\n' || key == KEY_ENTER)
        {
          input_buffer[input_length] = 0;
          submit_input(agent_stdin, input_buffer);
          input_length = 0;
          pending_length = 0;
        }
      else if (key >= 32 && key != 127
               && input_length + pending_length < sizeof(input_buffer) - 1
               && pending_length < sizeof(input_pending))
        {
          input_pending[pending_length++] = (unsigned char) key;
          if (fullrune((char *) input_pending, (int) pending_length))
            {
              memcpy(input_buffer + input_length, input_pending, pending_length);
              input_length += pending_length;
              pending_length = 0;
            }
        }
      draw_input(input_buffer, input_length);
    }
  if (agent_pid > 0)
    {
      kill(agent_pid, SIGTERM);
      waitpid(agent_pid, &status, 0);
    }
  if (agent_stdin >= 0)
    close(agent_stdin);
  if (agent_stdout >= 0)
    close(agent_stdout);
  destroy_layout();
  endwin();
  return 0;
}
