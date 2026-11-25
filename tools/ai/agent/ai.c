#define _GNU_SOURCE

#include <dirent.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <libgen.h>
#include <poll.h>
#include <sys/select.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "version.h"

#include "agent.h"
#include "tool.h"
#include "landlock.h"

#include "cJSON.h"

#include "editor.h"

#define VERSION "1.0"
#define DEFAULT_ENDPOINT "http://127.0.0.1:8090/v1/chat/completions"
#define DEFAULT_MODEL "glm-5.3"
#define SYSTEM_PROMPT                                                                                                  \
  "You are a senior Linux systems and software engineer working directly on the user's host. "                         \
  "Diagnose methodically, state assumptions and impact, prefer read-only commands first, "                             \
  "and finish with a concise conclusion. Request exactly one non-interactive terminal command "                        \
  "per tool call. Never launch interactive TUIs or editors. Disable pagers when supported "                            \
  "(for example --no-pager or PAGER=cat) and explicitly limit large output. Do not assume "                            \
  "working directory or shell environment persists across calls. Never conceal or misrepresent command "               \
  "output. Prefer the purpose-built file tools over shell equivalents: read_file for viewing file "                    \
  "contents, list_dir for inspecting directories, and apply_patch with unique exact SEARCH/REPLACE "                   \
  "blocks for creating, editing, or deleting files; use terminal commands for builds, searches, and "                  \
  "everything else. For any multi-step task, first record a checklist with the plan tool and keep it current "         \
  "as steps complete; the latest plan stays pinned in every later request. "                                           \
  "Stop and ask the human before using sudo, modifying system paths (/etc, /usr, /var), "                              \
  "restarting services, or performing any destructive, ambiguous, or out-of-scope action."

/* One-shot runs have no second turn: asking the human anything strands the
   task, so the model must settle for itself. */
#define ONE_SHOT_PROMPT                                                                                                \
  "One-shot mode: this prompt is the only user input and no follow-up answer will "                                     \
  "arrive. Do not end with questions or requests for confirmation; weigh the options, "                                 \
  "pick the most reasonable and safest course yourself, state the assumptions you make, "                               \
  "and carry the task to completion.\n"

#define MAX_ENDPOINT 512
#define MAX_HOST 128
#define MAX_PATH 384
#define MAX_KEY 512
#define MAX_LINE 8192
#define MAX_COMMAND 8192
#define MAX_PLAN 4096
#define MAX_ID 256
#define MAX_NAME 128
#define MAX_TOOL_CALLS 8
#define MAX_JOBS 16
#define MAX_MESSAGES 24
#define MAX_SUMMARY (8 * 1024)
#define MAX_SUMMARY_REQUEST (192 * 1024)
#define COMPACTION_MIN_ROUNDS 4
#define DEFAULT_TOOL_ROUNDS 128
#define MAX_TOOL_ROUNDS 1024
#define MAX_HTTP_ATTEMPTS 5
#define MAX_RETRY_AFTER 60
#define MESSAGE_SIZE (512 * 1024)
#define MAX_REQUEST (512 * 1024)
#define MAX_RESPONSE (256 * 1024)
#define CAPTURE_SIZE 65536
#define MODEL_OUTPUT_SIZE 3072
#define MAX_PROMPT_CONTEXT (64 * 1024)
#define MAX_INITIAL_PROMPT (384 * 1024)
#define MAX_TIMEOUT 1800
#define MAX_STEERING_LINES 64
#define MAX_SKILLS 16
#define MAX_SKILL_NAME 64
#define MAX_SKILL_DESC 512
#define MAX_SKILL_BODY 4096
#define SKILLS_DIR ".agents/skills"

typedef enum
{
  ROLE_SYSTEM,
  ROLE_USER,
  ROLE_ASSISTANT,
  ROLE_ASSISTANT_TOOLS,
  ROLE_TOOL
} MessageRole;

typedef struct
{
  char json[MESSAGE_SIZE];
  size_t length;
  MessageRole role;
} Message;

typedef struct
{
  char *data;
  size_t capacity;
  size_t length;
} Buffer;

typedef enum
{
  TOOL_UNKNOWN,
  TOOL_TERMINAL,
  TOOL_PLAN,
  TOOL_READ_FILE,
  TOOL_LIST_DIR,
  TOOL_APPLY_PATCH,
  TOOL_START_JOB,
  TOOL_JOB_STATUS,
  TOOL_STOP_JOB,
  TOOL_TASK,
} ToolKind;

typedef struct
{
  char id[MAX_ID];
  char name[MAX_NAME];
  char command[MAX_COMMAND];
  char checklist[MAX_PLAN];
  char path[PATH_MAX];
  char patch[MAX_PATCH];
  long offset;
  long limit;
  int job_id;
  ToolKind kind;
  int supported;
  int valid;
} ToolCall;

typedef struct
{
  char name[MAX_SKILL_NAME];
  char description[MAX_SKILL_DESC];
  char directory[MAX_SKILL_NAME];
} Skill;

static Message messages[MAX_MESSAGES];
static size_t message_count;
static char current_plan[MAX_PLAN];
static int plan_active;
static const char plan_header[] = "CURRENT TASK PLAN (recorded with the plan tool; follow it, "
                                  "tick steps only once command output confirms them, and "
                                  "rewrite it when the approach changes):\n";
static char context_summary[MAX_SUMMARY];
static int summary_active;
static long compaction_round = -COMPACTION_MIN_ROUNDS;
static int compaction_count;
static const char summary_header[] = "CONVERSATION SUMMARY (context compacted from earlier messages of "
                                     "this session; treat it as reliable):\n";
static const char summarize_prompt[] =
    "You maintain the running summary of an AI coding agent's session. Fold the appended "
    "conversation fragments into the existing summary. Keep the task goal, decisions made, "
    "files touched, commands with their outcomes, and open threads; drop settled detail. "
    "Reply with only the updated summary, plain text.";
static char endpoint[MAX_ENDPOINT] = DEFAULT_ENDPOINT;
static char api_host[MAX_HOST] = "127.0.0.1";
static char api_path[MAX_PATH] = "/v1/chat/completions";
static int api_port = 8090;
static char api_key[MAX_KEY];
static char model[MAX_MODEL] = DEFAULT_MODEL;
static const char *model_override;
static int api_timeout = 300;
static int command_timeout = 600;
static int timeout_override = -1;
static const char *exec_prompt;
static const char *workspace_directory;
static char workspace_root[PATH_MAX];
static char current_directory[PATH_MAX];
static long total_prompt_tokens;
static long total_completion_tokens;
static long last_prompt_tokens;
static long response_rounds;
static FILE *session_file;
static int session_save_failed;
static int session_replaying;
static int session_continue;
static const char *session_resume_path;
static char session_path[MAX_PATH];
static int one_shot;
static const char *attachment_paths[MAX_ATTACHMENTS];
static int attachment_count;
static int max_tool_rounds = DEFAULT_TOOL_ROUNDS;
static int streaming_enabled = 1;
static int stream_printed_content;
static int stream_terminated;
static int landlock_enabled = 1;
static int resolve_transient;
static bool full_access = false; // Always default to secure mode
/* --no-shell analyst mode: no terminal or job tools, file tools and plan
   only. Also forced onto TUI-mode subagents, whose approvals cannot be
   routed anywhere safe. */
static int no_shell;
/* Set in a delegated subagent (AI_SUBAGENT=1 in the environment): the
   task tool is not registered and transcripts stay off, so delegation
   never nests and never writes a second session file. */
static int subagent_depth;
/* Set while a shell-mode `!` command runs: the human typed it directly, so
   workspace scoping must not confine it. */
static int shell_direct;
static volatile sig_atomic_t interrupted;
static volatile sig_atomic_t agent_exit_status;
static int ui_mode;
static int terminal_styling;
static Skill skills[MAX_SKILLS];
static int skill_count;

/* Background jobs: run_command children without the foreground wait.
   Exited entries stay pollable until the table fills, then the oldest
   finished one is evicted for a new job. */
typedef enum
{
  JOB_RUNNING,
  JOB_EXITED,
  JOB_STOPPED
} JobState;

typedef struct
{
  int used;
  int id;
  pid_t pid;
  char command[MAX_COMMAND];
  char log_path[PATH_MAX + 32];
  time_t started;
  time_t finished;
  JobState state;
  int exit_status;
} Job;

static Job jobs[MAX_JOBS];
static int job_sequence;

typedef struct
{
  char text[MAX_COMMAND];
  int complete;
} SteeringLine;

static SteeringLine steering_input[MAX_STEERING_LINES];
static size_t steering_read_index;
static size_t steering_write_index;
static pthread_mutex_t steering_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
  struct termios original_terminal;
  char line[MAX_COMMAND];
  size_t line_length;
  size_t line_start;
  int input_flags;
  int terminal_configured;
  int input_open;
} LiveInput;

static LiveInput live_input;
static struct termios original_terminal;
static int terminal_state_saved;

static void
restore_terminal_state (void)
{
  if (terminal_state_saved)
    tcsetattr (STDIN_FILENO, TCSANOW, &original_terminal);
}

static void
on_signal (int signal_number)
{
  interrupted = 1;
  if (agent_exit_status == EXIT_SUCCESS)
    agent_exit_status = signal_number == SIGINT ? EXIT_CANCELLED : EXIT_FAILURE;
}

static int
hide_control_characters (void)
{
  struct termios terminal;

  if (!isatty (STDIN_FILENO) || tcgetattr (STDIN_FILENO, &original_terminal) != 0)
    return 0;
  terminal = original_terminal;
  terminal.c_lflag &= ~ECHOCTL;
  if (tcsetattr (STDIN_FILENO, TCSANOW, &terminal) != 0)
    return 0;
  terminal_state_saved = 1;
  return atexit (restore_terminal_state);
}

static int
take_steering (char *buffer, size_t capacity)
{
  int available = 0;

  pthread_mutex_lock (&steering_mutex);
  if (steering_input[steering_read_index].complete)
    {
      SteeringLine *line = &steering_input[steering_read_index];

      if (strlen (line->text) < capacity)
        {
          strcpy (buffer, line->text);
          available = 1;
        }
      line->complete = 0;
      steering_read_index = (steering_read_index + 1) % MAX_STEERING_LINES;
    }
  pthread_mutex_unlock (&steering_mutex);
  return available;
}

static int
append_live_steering (char *steer, size_t capacity)
{
  char line[MAX_COMMAND];
  size_t used;
  int appended = 0;

  if (capacity == 0)
    return -1;
  used = strlen (steer);
  while (take_steering (line, sizeof (line)))
    {
      size_t line_length = strlen (line);

      if (line_length == 0)
        continue;
      if (used != 0 && used + 1 >= capacity)
        return -1;
      if (line_length >= capacity - used - (used != 0 ? 1 : 0))
        return -1;
      if (used != 0)
        steer[used++] = '\n';
      memcpy (steer + used, line, line_length + 1);
      used += line_length;
      appended = 1;
    }
  return appended;
}

static int
write_all (int fd, const char *data, size_t length)
{
  while (length > 0)
    {
      ssize_t written = write (fd, data, length);
      if (written < 0)
        {
          if (errno == EINTR)
            continue;
          return -1;
        }
      if (written == 0)
        return -1;
      data += written;
      length -= (size_t)written;
    }
  return 0;
}

static const char *
terminal_style (const char *text)
{
  if (!terminal_styling)
    return NULL;
  while (*text == '\n' || *text == '\r')
    text++;
  if (strncmp (text, "error:", 6) == 0 || strncmp (text, "Error:", 6) == 0)
    return "\033[1;31m";
  if (strncmp (text, "[AI request", 12) == 0 || strncmp (text, "[live steering", 14) == 0
      || strncmp (text, "[command", 8) == 0 || strncmp (text, "[context", 8) == 0 || strncmp (text, "[skill]", 7) == 0
      || strncmp (text, "[shell", 6) == 0
      || strncmp (text, "[job", 4) == 0
      || strncmp (text, "[Plan", 5) == 0 || strncmp (text, "[force mode", 11) == 0
      || strncmp (text, "[retrying", 9) == 0 || strncmp (text, "[usage]", 7) == 0
      || strncmp (text, "[session]", 9) == 0 || strncmp (text, "[file read", 10) == 0
      || strncmp (text, "[dir listing", 12) == 0 || strncmp (text, "[patch applied", 14) == 0)
    return "\033[1;35m";
  if (strncmp (text, "[AI requests command]", 21) == 0 || strncmp (text, "[AI Response]", 13) == 0
      || strncmp (text, "[AI reads file]", 15) == 0 || strncmp (text, "[AI lists dir]", 14) == 0
      || strncmp (text, "[AI starts job]", 15) == 0 || strncmp (text, "[AI stops job]", 14) == 0
      || strncmp (text, "[AI requests patch]", 19) == 0)
    return "\033[1;36m";
  if (strncmp (text, "[You]", 5) == 0)
    return "\033[1;32m";
  return NULL;
}

/* True when the last byte written to stdout left the cursor mid-line. */
static int output_partial_line;

static void
print_formatted (const char *format, ...)
{
  char text[MAX_LINE];
  va_list arguments;
  const char *style;
  int length;

  va_start (arguments, format);
  length = vsnprintf (text, sizeof (text), format, arguments);
  va_end (arguments);
  if (length < 0)
    return;
  /* vsnprintf reports the untruncated length; clamp to what text holds. */
  if (length >= (int)sizeof (text))
    length = (int)sizeof (text) - 1;
  style = terminal_style (text);
  if (style != NULL)
    write_all (STDOUT_FILENO, style, strlen (style));
  if (write_all (STDOUT_FILENO, text, (size_t)length) < 0)
    {
      /* Output failures are not recoverable in this compact tool. */
    }
  else if (length > 0)
    output_partial_line = text[length - 1] != '\n';
  if (style != NULL)
    write_all (STDOUT_FILENO, "\033[0m", 4);
}

/* Status markers must start on their own line; streamed deltas or command
   output can leave the cursor mid-line, so step below that first. */
static void
print_line_break (void)
{
  if (output_partial_line)
    print_formatted ("\n");
}

static void
live_steering_prompt (void)
{
  print_formatted ("[AI request active]\n[live steering active]\n");
}

/* The terminal editor prints its prompts through this so labels keep
   their colors on redraws. */
static void
print_editor_prompt (const char *text)
{
  print_formatted ("%s", text);
}

/* Handed to the editor so its blocking read aborts on a SIGINT that
   landed before the read instead of waiting for the next keypress. */
static int
interrupt_is_pending (void)
{
  return interrupted;
}

static void
queue_live_line (const char *text, size_t length)
{
  SteeringLine *slot;
  int queued = 0;

  pthread_mutex_lock (&steering_mutex);
  slot = &steering_input[steering_write_index];
  if (!slot->complete)
    {
      memcpy (slot->text, text, length + 1);
      slot->complete = 1;
      steering_write_index = (steering_write_index + 1) % MAX_STEERING_LINES;
      queued = 1;
    }
  pthread_mutex_unlock (&steering_mutex);
  if (queued)
    print_formatted ("[live steering queued]: %s\n", text);
  else
    print_formatted ("[live steering dropped]: steering queue is full\n");
}

static void
erase_live_input (size_t length)
{
  /* Only the text after the last newline is on screen; erase and redraw
     that current line instead of the whole multi-line buffer. */
  if (length > live_input.line_length - live_input.line_start)
    length = live_input.line_length - live_input.line_start;
  if (length == 0)
    return;
  live_input.line_length -= length;
  live_input.line[live_input.line_length] = 0;
  if (live_input.terminal_configured)
    print_formatted ("\r\33[K%s", live_input.line + live_input.line_start);
}

static int
begin_live_input (void)
{
  struct termios terminal;
  int flags;

  live_input.line_length = 0;
  live_input.line_start = 0;
  live_input.terminal_configured = 0;
  if (one_shot && !isatty (STDIN_FILENO))
    {
      live_input.input_open = 0;
      return 0;
    }
  flags = fcntl (STDIN_FILENO, F_GETFL, 0);
  live_input.input_open = flags >= 0;
  if (!live_input.input_open)
    return 0;
  live_input.input_flags = flags;
  if (fcntl (STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) < 0)
    {
      live_input.input_open = 0;
      return 0;
    }
  if (tcgetattr (STDIN_FILENO, &live_input.original_terminal) != 0)
    return 0;
  terminal = live_input.original_terminal;
  /* The line is echoed by poll_live_input itself; keep the terminal
     driver from echoing every character a second time. */
  terminal.c_lflag &= ~(ICANON | ECHO | ECHONL);
  terminal.c_cc[VMIN] = 0;
  terminal.c_cc[VTIME] = 0;
  if (tcsetattr (STDIN_FILENO, TCSANOW, &terminal) != 0)
    return 0;
  live_input.terminal_configured = 1;
  return 0;
}

static void
poll_live_input (void)
{
  char character;
  int queued_input = 0;

  if (!live_input.input_open)
    return;
  while (read (STDIN_FILENO, &character, 1) == 1)
    {
      if (character == '\n' || character == '\r')
        {
          if (live_input.line_length == 0 && queued_input)
            {
              queue_live_line ("", 0);
              queued_input = 0;
            }
          else if (live_input.line_length != 0)
            {
              if (live_input.line_length < sizeof (live_input.line) - 1)
                live_input.line[live_input.line_length++] = '\n';
              live_input.line[live_input.line_length] = 0;
              queued_input = 1;
              live_input.line_start = live_input.line_length;
              if (live_input.terminal_configured)
                print_formatted ("\r\n");
            }
        }
      else if (character == 127 || character == 8)
        erase_live_input (1);
      else if (character == 4)
        erase_live_input (live_input.line_length);
      else if (character == 21)
        {
          size_t start = live_input.line_length;

          while (start != live_input.line_start
                 && live_input.line[start - 1] == ' ')
            start--;
          while (start != live_input.line_start
                 && live_input.line[start - 1] != ' '
                 && live_input.line[start - 1] != '\n')
            start--;
          erase_live_input (live_input.line_length - start);
        }
      else if (live_input.terminal_configured && character == 2)
        ;
      else if (live_input.terminal_configured && character == 6)
        ;
      else if (live_input.line_length < sizeof (live_input.line) - 1)
        {
          live_input.line[live_input.line_length++] = character;
          if (live_input.terminal_configured)
            write_all (STDOUT_FILENO, &character, 1);
        }
    }
}

static void
end_live_input (void)
{
  if (live_input.line_length != 0)
    {
      live_input.line[live_input.line_length] = 0;
      /* A partial line is still on screen; move below it before the
         queued notice reprints it. */
      if (live_input.terminal_configured
          && live_input.line_length != live_input.line_start)
        print_formatted ("\r\n");
      queue_live_line (live_input.line, live_input.line_length);
      live_input.line_length = 0;
    }
  live_input.line_start = 0;
  if (live_input.terminal_configured)
    tcsetattr (STDIN_FILENO, TCSANOW, &live_input.original_terminal);
  fcntl (STDIN_FILENO, F_SETFL, live_input.input_flags);
  live_input.terminal_configured = 0;
  live_input.input_open = 0;
}

static int
steering_available (void)
{
  int available;

  pthread_mutex_lock (&steering_mutex);
  available = steering_input[steering_read_index].complete;
  pthread_mutex_unlock (&steering_mutex);
  return available;
}

static void
clear_steering (void)
{
  pthread_mutex_lock (&steering_mutex);
  memset (steering_input, 0, sizeof (steering_input));
  steering_read_index = 0;
  steering_write_index = 0;
  pthread_mutex_unlock (&steering_mutex);
}

static int
buffer_append (Buffer *buffer, const char *data, size_t length)
{
  if (length > buffer->capacity - buffer->length)
    return -1;
  memcpy (buffer->data + buffer->length, data, length);
  buffer->length += length;
  return 0;
}

static int
buffer_append_string (Buffer *buffer, const char *data)
{
  return buffer_append (buffer, data, strlen (data));
}

__attribute__ ((format (printf, 2, 3))) static int
buffer_append_formatted (Buffer *buffer, const char *format, ...)
{
  char text[MAX_LINE];
  va_list arguments;
  int length;

  va_start (arguments, format);
  length = vsnprintf (text, sizeof (text), format, arguments);
  va_end (arguments);
  if (length < 0)
    return -1;
  return buffer_append (buffer, text, (size_t)length);
}

static int
append_json_string_body (Buffer *buffer, const char *data, size_t length)
{
  size_t index;

  for (index = 0; index < length; index++)
    {
      unsigned char byte = (unsigned char)data[index];
      char escaped[7];

      if (byte == '"' || byte == '\\')
        {
          escaped[0] = '\\';
          escaped[1] = (char)byte;
          if (buffer_append (buffer, escaped, 2) < 0)
            return -1;
        }
      else if (byte == '\b' || byte == '\f' || byte == '\n' || byte == '\r' || byte == '\t')
        {
          escaped[0] = '\\';
          escaped[1] = (char)(byte == '\b' ? 'b' : byte == '\f' ? 'f' : byte == '\n' ? 'n' : byte == '\r' ? 'r' : 't');
          if (buffer_append (buffer, escaped, 2) < 0)
            return -1;
        }
      else if (byte < 0x20)
        {
          snprintf (escaped, sizeof (escaped), "\\u%04x", (unsigned int)byte);
          if (buffer_append (buffer, escaped, 6) < 0)
            return -1;
        }
      else if (buffer_append (buffer, (const char *)&data[index], 1) < 0)
        return -1;
    }
  return 0;
}

static int
append_json_string (Buffer *buffer, const char *data, size_t length)
{
  if (buffer_append_string (buffer, "\"") < 0 || append_json_string_body (buffer, data, length) < 0
      || buffer_append_string (buffer, "\"") < 0)
    return -1;
  return 0;
}

static int
append_json_text (Buffer *buffer, const char *data)
{
  return append_json_string (buffer, data, strlen (data));
}

static int
append_xml_escaped (Buffer *buffer, const char *data, size_t length)
{
  size_t index;

  for (index = 0; index < length; index++)
    {
      const char *replacement = NULL;

      switch (data[index])
        {
        case '&':
          replacement = "&amp;";
          break;
        case '<':
          replacement = "&lt;";
          break;
        case '>':
          replacement = "&gt;";
          break;
        case '\"':
          replacement = "&quot;";
          break;
        case '\'':
          replacement = "&apos;";
          break;
        default:
          if (buffer_append (buffer, data + index, 1) < 0)
            return -1;
          continue;
        }
      if (buffer_append_string (buffer, replacement) < 0)
        return -1;
    }
  return 0;
}

static int
append_xml_text (Buffer *buffer, const char *data)
{
  return append_xml_escaped (buffer, data, strlen (data));
}

/* Index range of the oldest prunable message unit: system messages stay,
   the last user message stays (the endpoint rejects a conversation without
   any user message, HTTP 400), and an assistant tool call is taken together
   with its tool replies so no reply is left orphaned. Returns 0 when
   nothing can be dropped. */
static int
oldest_drop_range (size_t *start_out, size_t *end_out)
{
  size_t index;
  size_t user_count = 0;

  for (index = 0; index < message_count; index++)
    if (messages[index].role == ROLE_USER)
      user_count++;
  for (index = 0; index < message_count; index++)
    {
      MessageRole role = messages[index].role;
      size_t end = index + 1;

      if (role == ROLE_SYSTEM || (role == ROLE_USER && user_count == 1))
        continue;
      if (role == ROLE_ASSISTANT_TOOLS)
        while (end < message_count && messages[end].role == ROLE_TOOL)
          end++;
      *start_out = index;
      *end_out = end;
      return 1;
    }
  return 0;
}

static void
drop_oldest_message (void)
{
  size_t start;
  size_t end;

  if (!oldest_drop_range (&start, &end))
    return;
  memmove (messages + start, messages + end, (message_count - end) * sizeof (messages[0]));
  message_count -= end - start;
  /* The drop can fire right after raw mid-line output, like a job_status
     log tail; status markers start on their own line. */
  print_line_break ();
  print_formatted ("[context] Dropped the oldest non-system message.\n");
}

/* Session persistence: every non-system message is appended, one JSON
   object per line, so a restart can replay the conversation. The file is
   opened lazily on the first logged message and flushed per line, which
   keeps a kill from losing more than the in-flight line. */
static void
session_open (const char *mode)
{
  int needs_separator = 0;

  /* A subagent's transcript is the task tool result in the parent's
     session; a second writer appending to the same file is corruption
     risk for no benefit. */
  if (session_file != NULL || session_save_failed || subagent_depth)
    return;
  mkdir (".ai", 0777);
  if (mode[0] == 'a')
    {
      /* A kill can leave a half-written final line; appending straight
         after it would glue two records together. */
      FILE *probe = fopen (session_path, "rb");

      if (probe != NULL)
        {
          if (fseek (probe, -1, SEEK_END) == 0 && fgetc (probe) != '\n')
            needs_separator = 1;
          fclose (probe);
        }
    }
  session_file = fopen (session_path, mode);
  if (session_file == NULL)
    {
      session_save_failed = 1;
      print_formatted ("[session] cannot open %s for saving: %s\n", session_path, strerror (errno));
      return;
    }
  if (needs_separator)
    fputc ('\n', session_file);
}

/* /new starts a fresh conversation, so the transcript restarts too;
   --continue then replays what was on screen, not replaced context. */
static void
session_restart (void)
{
  if (session_file != NULL)
    fclose (session_file);
  session_file = NULL;
  session_save_failed = 0;
  session_open ("w");
}

static void
session_write_line (const char *line, size_t length)
{
  session_open ("a");
  if (session_file == NULL)
    return;
  if (fwrite (line, 1, length, session_file) != length || fputc ('\n', session_file) == EOF
      || fflush (session_file) != 0 || fdatasync (fileno (session_file)) < 0)
    {
      print_formatted ("[session] write failed; saving disabled: %s\n", strerror (errno));
      fclose (session_file);
      session_file = NULL;
      session_save_failed = 1;
    }
}

static void
session_log_plan (const char *checklist)
{
  char line[2 * MAX_PLAN + 32];
  Buffer buffer = { line, sizeof (line), 0 };

  if (session_replaying || session_save_failed)
    return;
  if (buffer_append_string (&buffer, "{\"role\":\"_plan\",\"content\":") < 0
      || append_json_text (&buffer, checklist) < 0 || buffer_append_string (&buffer, "}") < 0)
    return;
  session_write_line (line, buffer.length);
}

/* Compaction rewrote history, so the transcript is rebuilt from the live
   conversation: --continue then resumes the compacted session instead of
   resurrecting the pruned bloat. */
static void
session_rewrite_compacted (void)
{
  char summary_line[2 * MAX_SUMMARY + 32];
  Buffer summary_buffer = { summary_line, sizeof (summary_line), 0 };
  size_t index;

  if (session_save_failed || subagent_depth)
    return;
  if (session_file != NULL)
    fclose (session_file);
  session_file = NULL;
  session_open ("w");
  if (session_file == NULL)
    return;
  if (summary_active)
    {
      if (buffer_append_string (&summary_buffer, "{\"role\":\"_summary\",\"content\":") < 0
          || append_json_text (&summary_buffer, context_summary) < 0
          || buffer_append_string (&summary_buffer, "}") < 0)
        return;
      session_write_line (summary_line, summary_buffer.length);
    }
  if (plan_active)
    session_log_plan (current_plan);
  for (index = 0; index < message_count; index++)
    if (messages[index].role != ROLE_SYSTEM)
      session_write_line (messages[index].json, messages[index].length);
}

static int compact_messages (size_t start, size_t end);

static int
add_message (const char *json, size_t length, MessageRole role)
{
  /* Pruning would delete the oldest turn outright; try to fold it into the
     pinned summary first so the task thread survives the window. Failure
     falls through to the plain drop below — compaction must never cost the
     session. Rate-limited: one summarize attempt per few rounds, success or
     not, so a failing endpoint is not re-queried on every later message. */
  if (message_count == MAX_MESSAGES && !session_replaying
      && response_rounds - compaction_round >= COMPACTION_MIN_ROUNDS)
    {
      size_t start;
      size_t end;

      if (oldest_drop_range (&start, &end))
        {
          compaction_round = response_rounds;
          compact_messages (start, end);
        }
    }
  while (message_count == MAX_MESSAGES || length > MESSAGE_SIZE)
    {
      size_t old_count = message_count;
      drop_oldest_message ();
      if (old_count == message_count)
        return -1;
    }
  memcpy (messages[message_count].json, json, length);
  messages[message_count].length = length;
  messages[message_count].role = role;
  message_count++;
  if (role != ROLE_SYSTEM && !session_replaying)
    session_write_line (json, length);
  return 0;
}

static int
add_text_message (const char *role, const char *content, int system)
{
  char json[MESSAGE_SIZE];
  Buffer buffer = { json, sizeof (json), 0 };

  if (buffer_append_formatted (&buffer, "{\"role\":") < 0 || append_json_text (&buffer, role) < 0
      || buffer_append_string (&buffer, ",\"content\":") < 0 || append_json_text (&buffer, content) < 0
      || buffer_append_string (&buffer, "}") < 0)
    return -1;
  return add_message (json, buffer.length, system ? ROLE_SYSTEM : ROLE_USER);
}

static void
trim_spaces (char *text)
{
  size_t length = strlen (text);
  size_t start = 0;

  while (start < length && (text[start] == ' ' || text[start] == '\t'))
    start++;
  while (
      length > start
      && (text[length - 1] == ' ' || text[length - 1] == '\t' || text[length - 1] == '\r' || text[length - 1] == '\n'))
    length--;
  memmove (text, text + start, length - start);
  text[length - start] = 0;
}

static void
strip_quotes (char *text)
{
  size_t length = strlen (text);

  if (length < 2)
    return;
  if ((text[0] == '"' && text[length - 1] == '"') || (text[0] == '\'' && text[length - 1] == '\''))
    {
      memmove (text, text + 1, length - 2);
      text[length - 2] = 0;
    }
}

static void
copy_skill_field (char *destination, size_t capacity, char *value)
{
  trim_spaces (value);
  strip_quotes (value);
  snprintf (destination, capacity, "%s", value);
}

static int
read_skill (const char *directory)
{
  char path[MAX_PATH];
  char line[1024];
  Skill *skill;
  FILE *file;
  int frontmatter = 0;

  snprintf (path, sizeof (path), "%s/%s/SKILL.md", SKILLS_DIR, directory);
  file = fopen (path, "r");
  if (file == NULL)
    return -1;
  skill = &skills[skill_count];
  memset (skill, 0, sizeof (*skill));
  strncpy (skill->name, directory, sizeof (skill->name) - 1);
  strncpy (skill->directory, directory, sizeof (skill->directory) - 1);
  while (fgets (line, sizeof (line), file) != NULL)
    {
      trim_spaces (line);
      if (strcmp (line, "---") == 0)
        {
          if (frontmatter)
            break;
          frontmatter = 1;
          continue;
        }
      if (!frontmatter)
        continue;
      if (strncmp (line, "name:", 5) == 0)
        copy_skill_field (skill->name, sizeof (skill->name), line + 5);
      else if (strncmp (line, "description:", 12) == 0)
        copy_skill_field (skill->description, sizeof (skill->description), line + 12);
    }
  fclose (file);
  return 0;
}

static void
load_skills (void)
{
  DIR *directory = opendir (SKILLS_DIR);
  struct dirent *entry;
  int index;

  if (directory == NULL)
    return;
  while (skill_count < MAX_SKILLS && (entry = readdir (directory)) != NULL)
    {
      if (entry->d_name[0] == '.')
        continue;
      if (strlen (entry->d_name) >= MAX_SKILL_NAME)
        continue;
      if (read_skill (entry->d_name) == 0)
        skill_count++;
    }
  closedir (directory);
  for (index = 1; index < skill_count; index++)
    {
      Skill key = skills[index];
      int sorted;

      for (sorted = index - 1; sorted >= 0 && strcmp (skills[sorted].name, key.name) > 0; sorted--)
        skills[sorted + 1] = skills[sorted];
      skills[sorted + 1] = key;
    }
}

static Skill *
find_skill (const char *name)
{
  int index;

  for (index = 0; index < skill_count; index++)
    if (strcmp (skills[index].name, name) == 0 || strcmp (skills[index].directory, name) == 0)
      return &skills[index];
  return NULL;
}

static int
add_skills_message (void)
{
  char content[MESSAGE_SIZE];
  Buffer buffer = { content, sizeof (content), 0 };
  int index;

  if (skill_count == 0)
    return 0;
  if (buffer_append_string (&buffer, "Skills available under " SKILLS_DIR "/<skill>/SKILL.md:\n") < 0)
    return -1;
  for (index = 0; index < skill_count; index++)
    {
      if (strcmp (skills[index].name, skills[index].directory) == 0)
        {
          if (buffer_append_formatted (&buffer, "- %s: %s\n", skills[index].name, skills[index].description) < 0)
            return -1;
        }
      else if (buffer_append_formatted (&buffer, "- %s (%s): %s\n", skills[index].name, skills[index].directory,
                                        skills[index].description)
               < 0)
        return -1;
    }
  if (buffer_append_string (&buffer, "When a task matches a skill, read its SKILL.md "
                                     "with a read-only command and follow its "
                                     "instructions before acting.")
      < 0)
    return -1;
  if (buffer.length >= sizeof (content))
    return -1;
  content[buffer.length] = 0;
  return add_text_message ("system", content, 1);
}

static void
handle_skill_command (char *name)
{
  char path[MAX_PATH];
  char body[MAX_SKILL_BODY + 16];
  Skill *skill;
  FILE *file;
  size_t length;

  trim_spaces (name);
  skill = find_skill (name);
  if (skill == NULL)
    {
      print_formatted ("error: unknown skill; /skills lists them\n");
      return;
    }
  snprintf (path, sizeof (path), "%s/%s/SKILL.md", SKILLS_DIR, skill->directory);
  file = fopen (path, "r");
  if (file == NULL)
    {
      print_formatted ("error: cannot open %s\n", path);
      return;
    }
  length = fread (body, 1, MAX_SKILL_BODY, file);
  fclose (file);
  body[length] = 0;
  if (length == MAX_SKILL_BODY)
    snprintf (body + length, sizeof (body) - length, "...[truncated]");
  if (add_text_message ("system", body, 1) < 0)
    {
      print_formatted ("error: skill cannot fit in the conversation\n");
      return;
    }
  print_formatted ("[skill] %s added to the context\n", skill->name);
}

static int
add_workspace_message (void)
{
  const char *directory = workspace_directory != NULL ? workspace_root : current_directory;
  char content[PATH_MAX + 128];

  if (workspace_directory != NULL)
    snprintf (content, sizeof (content),
              "Working directory: %s. The workspace is scoped; commands that reference paths outside it are rejected.\n",
              directory);
  else
    snprintf (content, sizeof (content), "Working directory: %s. Commands run from this directory.\n", directory);
  return add_text_message ("system", content, 1);
}

static void
conversation_reset (void)
{
  message_count = 0;
  plan_active = 0;
  current_plan[0] = 0;
  summary_active = 0;
  context_summary[0] = 0;
  if (add_text_message ("system", SYSTEM_PROMPT, 1) < 0 || add_workspace_message () < 0
      /* One-shot runs and the task-tool children they spawn learn the mode
         here; interactive conversations never do. */
      || (one_shot && add_text_message ("system", ONE_SHOT_PROMPT, 1) < 0)
      || add_skills_message () < 0)
    {
      print_formatted ("error: unable to initialize conversation\n");
      exit (1);
    }
}

static int
parse_positive_integer (const char *text, int maximum)
{
  char *end;
  long value;
  int conversion_error;

  errno = 0;
  value = strtol (text, &end, 10);
  conversion_error = errno == ERANGE;
  if (conversion_error || end == text || *end != 0 || value <= 0 || value > maximum)
    return -1;
  return (int)value;
}

/* Like parse_positive_integer, but 0 is accepted and means unlimited. */
static int
parse_timeout_seconds (const char *text, int maximum)
{
  char *end;
  long value;

  errno = 0;
  value = strtol (text, &end, 10);
  if (errno == ERANGE || end == text || *end != 0 || value < 0 || value > maximum)
    return -1;
  return (int)value;
}

static int
valid_host (const char *host, size_t length)
{
  size_t index;

  if (length == 0 || length >= MAX_HOST)
    return 0;
  for (index = 0; index < length; index++)
    if (!isalnum ((unsigned char)host[index]) && host[index] != '.' && host[index] != '-')
      return 0;
  return 1;
}

static int
configure_endpoint (const char *value)
{
  const char *host;
  const char *slash;
  const char *end;
  const char *port;
  size_t host_length;
  size_t path_length;
  long number;
  char *parse_end;
  int conversion_error;

  if (value == NULL || strncmp (value, "http://", 7) != 0)
    {
      print_formatted ("error: AI_API_URL must be an http:// URL\n");
      return -1;
    }
  host = value + 7;
  slash = strchr (host, '/');
  end = slash != NULL ? slash : host + strlen (host);
  port = memchr (host, ':', (size_t)(end - host));
  if (port != NULL)
    {
      host_length = (size_t)(port - host);
      port++;
      errno = 0;
      number = strtol (port, &parse_end, 10);
      conversion_error = errno == ERANGE;
      if (parse_end != end || conversion_error || number <= 0 || number > 65535)
        {
          print_formatted ("error: AI_API_URL port is invalid\n");
          return -1;
        }
      api_port = (int)number;
    }
  else
    {
      host_length = (size_t)(end - host);
      api_port = 80;
    }
  if (host_length == 0 || host_length >= sizeof (api_host))
    {
      print_formatted ("error: AI_API_URL host is too long\n");
      return -1;
    }
  memcpy (api_host, host, host_length);
  api_host[host_length] = 0;
  if (!valid_host (api_host, host_length))
    {
      print_formatted ("error: endpoint host must be an IPv4 address or hostname\n");
      return -1;
    }
  path_length = slash != NULL ? strlen (slash) : 0;
  if (path_length == 0)
    strcpy (api_path, "/v1/chat/completions");
  else if (path_length >= sizeof (api_path))
    {
      print_formatted ("error: AI_API_URL path is too long\n");
      return -1;
    }
  else
    strcpy (api_path, slash);
  if (strcmp (api_path, "/") == 0)
    strcpy (api_path, "/v1/chat/completions");
  else if (strcmp (api_path, "/v1") == 0)
    {
      if (path_length + strlen ("chat/completions") >= sizeof (api_path))
        {
          print_formatted ("error: AI_API_URL path is too long\n");
          return -1;
        }
      if (api_path[path_length - 1] == '/')
        memcpy (api_path + path_length, "chat/completions", 17);
      else
        memcpy (api_path + path_length, "/chat/completions", 18);
    }
  else if (path_length > 1 && api_path[path_length - 1] == '/')
    {
      if (path_length + strlen ("chat/completions") >= sizeof (api_path))
        {
          print_formatted ("error: AI_API_URL path is too long\n");
          return -1;
        }
      memcpy (api_path + path_length, "chat/completions", 17);
    }
  if (strlen (value) >= sizeof (endpoint))
    {
      print_formatted ("error: AI_API_URL is too long\n");
      return -1;
    }
  strcpy (endpoint, value);
  return 0;
}

static int
valid_header_value (const char *value)
{
  size_t index;

  for (index = 0; value[index] != 0; index++)
    if (value[index] < 0x21 || value[index] > 0x7e)
      return 0;
  return 1;
}

static int
configure_environment (void)
{
  const char *value;
  const char *selected_key;
  int timeout;

  value = getenv ("AI_API_URL");
  if (value == NULL)
    value = DEFAULT_ENDPOINT;
  if (configure_endpoint (value) < 0)
    return -1;
  value = getenv ("AI_MODEL");
  if (value != NULL && model_override == NULL)
    {
      if (value[0] == 0 || strlen (value) >= sizeof (model))
        {
          print_formatted ("error: AI_MODEL is invalid\n");
          return -1;
        }
      strcpy (model, value);
    }
  if (model_override != NULL)
    strcpy (model, model_override);
  selected_key = getenv ("AI_API_KEY");
  if (selected_key == NULL)
    selected_key = getenv ("OPENAI_API_KEY");
  if (selected_key != NULL)
    {
      if (strlen (selected_key) >= sizeof (api_key) || !valid_header_value (selected_key))
        {
          print_formatted ("error: API key is invalid\n");
          return -1;
        }
      strcpy (api_key, selected_key);
    }
  value = getenv ("AI_TIMEOUT_SECONDS");
  if (value != NULL)
    {
      timeout = parse_positive_integer (value, MAX_TIMEOUT);
      if (timeout < 0)
        {
          print_formatted ("error: AI_TIMEOUT_SECONDS is invalid\n");
          return -1;
        }
      api_timeout = timeout;
    }
  value = getenv ("AI_COMMAND_TIMEOUT_SECONDS");
  if (value != NULL)
    {
      timeout = parse_positive_integer (value, MAX_TIMEOUT);
      if (timeout < 0)
        {
          print_formatted ("error: AI_COMMAND_TIMEOUT_SECONDS is invalid\n");
          return -1;
        }
      command_timeout = timeout;
    }
  if (timeout_override >= 0)
    command_timeout = timeout_override;
  value = getenv ("AI_TOOL_ROUNDS");
  if (value != NULL)
    {
      timeout = parse_positive_integer (value, MAX_TOOL_ROUNDS);
      if (timeout < 0)
        {
          print_formatted ("error: AI_TOOL_ROUNDS is invalid\n");
          return -1;
        }
      max_tool_rounds = timeout;
    }
  value = getenv ("AI_STREAM");
  if (value != NULL)
    streaming_enabled = strcmp (value, "0") != 0;
  value = getenv ("AI_LANDLOCK");
  if (value != NULL)
    landlock_enabled = strcmp (value, "0") != 0;
  subagent_depth = getenv ("AI_SUBAGENT") != NULL;
  return 0;
}

static int
copy_json_string (const cJSON *item, char *output, size_t capacity)
{
  const char *value;

  if (!cJSON_IsString (item) || capacity == 0)
    return -1;
  value = cJSON_GetStringValue (item);
  if (strlen (value) >= capacity)
    return -1;
  strcpy (output, value);
  return 0;
}

static int
header_equal (const char *line, size_t length, const char *name)
{
  size_t name_length = strlen (name);

  return length > name_length && strncasecmp (line, name, name_length) == 0 && line[name_length] == ':';
}

/* Replay a saved session into the live conversation. Stored lines are the
   exact message objects previously sent, so they are re-added verbatim;
   system lines are skipped because conversation_reset rebuilds that context
   fresh each run, and a torn or malformed line from a kill is skipped rather
   than fatal. add_message applies the MAX_MESSAGES drop-oldest window as
   usual, so an oversized log still replays its newest tail. */
static int
resume_session (const char *path)
{
  static char line[MESSAGE_SIZE + 8];
  size_t restored = 0;
  size_t skipped = 0;
  FILE *file;

  file = fopen (path, "r");
  if (file == NULL)
    return -1;
  session_replaying = 1;
  while (fgets (line, sizeof (line), file) != NULL)
    {
      size_t length = strlen (line);
      cJSON *item;
      const char *role;
      MessageRole message_role;

      while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
        line[--length] = 0;
      if (length == 0)
        continue;
      item = cJSON_Parse (line);
      role = cJSON_GetStringValue (cJSON_GetObjectItemCaseSensitive (item, "role"));
      if (role == NULL)
        skipped++;
      else if (strcmp (role, "_plan") == 0)
        {
          if (copy_json_string (cJSON_GetObjectItemCaseSensitive (item, "content"), current_plan,
                                sizeof (current_plan))
              == 0)
            plan_active = current_plan[0] != 0;
        }
      else if (strcmp (role, "_summary") == 0)
        {
          if (copy_json_string (cJSON_GetObjectItemCaseSensitive (item, "content"), context_summary,
                                sizeof (context_summary))
              == 0)
            summary_active = context_summary[0] != 0;
        }
      else
        {
          if (strcmp (role, "user") == 0)
            message_role = ROLE_USER;
          else if (strcmp (role, "assistant") == 0)
            message_role = strstr (line, "\"tool_calls\"") != NULL ? ROLE_ASSISTANT_TOOLS : ROLE_ASSISTANT;
          else if (strcmp (role, "tool") == 0)
            message_role = ROLE_TOOL;
          else
            message_role = ROLE_SYSTEM;
          if (message_role == ROLE_SYSTEM || add_message (line, length, message_role) < 0)
            skipped++;
          else
            restored++;
        }
      cJSON_Delete (item);
    }
  session_replaying = 0;
  fclose (file);
  print_formatted ("[session] resumed %zu messages from %s\n", restored, path);
  return 0;
}

/* Streaming (SSE) response handling. When the endpoint answers with
   "text/event-stream" the deltas are printed as they arrive and the final
   message is reassembled into the same JSON shape the buffered path parses,
   so conversation state, session logs, and tool dispatch stay identical.
   Output discipline: the worker thread only accumulates bytes; the main
   thread is the sole writer to stdout, flushing new deltas between input
   polls and once more after the worker joins. */

#define MAX_SSE_EVENT (128 * 1024)
#define MAX_STREAM_ARGUMENTS (MAX_PATCH + MAX_COMMAND + 1024)

static char stream_content[MAX_LINE];
static volatile size_t stream_available;
static volatile size_t stream_flushed;
static volatile sig_atomic_t stream_active;

typedef struct
{
  int chunked;
  size_t chunk_remaining;
  int chunk_header;
  int chunk_terminator;
  char line[MAX_SSE_EVENT];
  size_t line_length;
  char size[32];
  size_t size_length;
  char event[MAX_SSE_EVENT];
  size_t event_length;
} StreamScan;

typedef struct
{
  char id[MAX_ID];
  char name[MAX_NAME];
  size_t name_length;
  char arguments[MAX_STREAM_ARGUMENTS];
  size_t arguments_length;
} StreamToolCall;

typedef struct
{
  StreamScan scan;
  StreamToolCall calls[MAX_TOOL_CALLS];
  size_t call_count;
  size_t content_length;
  long prompt_tokens;
  long completion_tokens;
  int have_usage;
  int done;
  int failed;
} StreamState;

static int
stream_append_data (char *buffer, size_t *length, size_t capacity, const char *data, size_t data_length)
{
  if (*length >= capacity || data_length >= capacity - *length)
    return -1;
  memcpy (buffer + *length, data, data_length);
  *length += data_length;
  buffer[*length] = 0;
  return 0;
}

/* Handle one complete SSE event: the accumulated `data:` payload. */
static void
stream_dispatch (StreamState *state)
{
  StreamScan *scan = &state->scan;
  cJSON *event;
  const cJSON *choices;
  const cJSON *choice;
  const cJSON *delta;
  const cJSON *item;
  const cJSON *usage;
  char *text;

  if (scan->event_length == 0)
    return;
  if (scan->event[scan->event_length - 1] == '\n')
    scan->event_length--;
  scan->event[scan->event_length] = 0;
  if (strcmp (scan->event, "[DONE]") == 0)
    {
      state->done = 1;
      return;
    }
  event = cJSON_Parse (scan->event);
  if (event == NULL)
    return; /* Not JSON (a keep-alive ping); resync at the next event. */
  item = cJSON_GetObjectItemCaseSensitive (event, "error");
  if (cJSON_IsObject (item))
    {
      char message[MAX_LINE];
      const cJSON *detail = cJSON_GetObjectItemCaseSensitive (item, "message");

      if (copy_json_string (detail, message, sizeof (message)) == 0)
        print_formatted ("error: AI endpoint: %s\n", message);
      else
        print_formatted ("error: AI endpoint returned an error response\n");
      cJSON_Delete (event);
      state->failed = 1;
      return;
    }
  choices = cJSON_GetObjectItemCaseSensitive (event, "choices");
  choice = cJSON_IsArray (choices) ? cJSON_GetArrayItem (choices, 0) : NULL;
  delta = choice != NULL ? cJSON_GetObjectItemCaseSensitive (choice, "delta") : NULL;
  if (delta != NULL)
    {
      item = cJSON_GetObjectItemCaseSensitive (delta, "content");
      if (cJSON_IsString (item))
        {
          text = cJSON_GetStringValue (item);
          if (text[0] != 0
              && stream_append_data (stream_content, &state->content_length, sizeof (stream_content), text,
                                     strlen (text)) == 0)
            stream_available = state->content_length;
          else if (text[0] != 0)
            {
              print_formatted ("error: assistant content is too long or malformed\n");
              cJSON_Delete (event);
              state->failed = 1;
              return;
            }
        }
      item = cJSON_GetObjectItemCaseSensitive (delta, "tool_calls");
      if (cJSON_IsArray (item))
        {
          const cJSON *entry;

          cJSON_ArrayForEach (entry, item)
          {
            const cJSON *index_item = cJSON_GetObjectItemCaseSensitive (entry, "index");
            const cJSON *id_item;
            const cJSON *function;
            StreamToolCall *call;
            int index = 0;

            if (cJSON_IsNumber (index_item))
              index = (int)index_item->valuedouble;
            if (index < 0 || index >= MAX_TOOL_CALLS)
              continue;
            if ((size_t)index >= state->call_count)
              state->call_count = (size_t)index + 1;
            call = &state->calls[index];
            id_item = cJSON_GetObjectItemCaseSensitive (entry, "id");
            if (cJSON_IsString (id_item))
              {
                text = cJSON_GetStringValue (id_item);
                if (text[0] != 0)
                  snprintf (call->id, sizeof (call->id), "%s", text);
              }
            function = cJSON_GetObjectItemCaseSensitive (entry, "function");
            if (cJSON_IsObject (function))
              {
                const cJSON *fragment = cJSON_GetObjectItemCaseSensitive (function, "name");

                if (cJSON_IsString (fragment))
                  {
                    text = cJSON_GetStringValue (fragment);
                    if (text[0] != 0
                        && stream_append_data (call->name, &call->name_length, sizeof (call->name), text,
                                               strlen (text)) < 0)
                      {
                        print_formatted ("error: AI stream tool name is too long\n");
                        cJSON_Delete (event);
                        state->failed = 1;
                        return;
                      }
                  }
                fragment = cJSON_GetObjectItemCaseSensitive (function, "arguments");
                if (cJSON_IsString (fragment))
                  {
                    text = cJSON_GetStringValue (fragment);
                    if (text[0] != 0
                        && stream_append_data (call->arguments, &call->arguments_length, sizeof (call->arguments),
                                               text, strlen (text)) < 0)
                      {
                        print_formatted ("error: AI stream tool arguments exceed limit\n");
                        cJSON_Delete (event);
                        state->failed = 1;
                        return;
                      }
                  }
              }
          }
        }
    }
  usage = cJSON_GetObjectItemCaseSensitive (event, "usage");
  if (cJSON_IsObject (usage))
    {
      const cJSON *number;

      number = cJSON_GetObjectItemCaseSensitive (usage, "prompt_tokens");
      if (cJSON_IsNumber (number))
        state->prompt_tokens = (long)number->valuedouble;
      number = cJSON_GetObjectItemCaseSensitive (usage, "completion_tokens");
      if (cJSON_IsNumber (number))
        state->completion_tokens = (long)number->valuedouble;
      state->have_usage = 1;
    }
  cJSON_Delete (event);
}

/* Handle one completed SSE line (newline consumed, CR not yet stripped). */
static void
stream_feed_line (StreamState *state, const char *line, size_t length)
{
  StreamScan *scan = &state->scan;

  if (length > 0 && line[length - 1] == '\r')
    length--;
  if (length == 0)
    {
      if (scan->event_length > 0)
        {
          stream_dispatch (state);
          scan->event_length = 0;
        }
      return;
    }
  if (line[0] == ':')
    return; /* Comment, e.g. a keep-alive ping. */
  if (length > 5 && memcmp (line, "data:", 5) == 0)
    {
      size_t skip = line[5] == ' ' ? 6 : 5;

      if (stream_append_data (scan->event, &scan->event_length, sizeof (scan->event), line + skip,
                             length - skip) < 0
          || stream_append_data (scan->event, &scan->event_length, sizeof (scan->event), "\n", 1) < 0)
        {
          print_formatted ("error: AI stream event exceeds limit\n");
          state->failed = 1;
        }
      return;
    }
  /* event:, id:, and retry: fields carry nothing this client needs. */
}

/* Dechunk (when needed) and split raw body bytes into SSE lines. */
static void
stream_feed_bytes (StreamState *state, const char *data, size_t length)
{
  StreamScan *scan = &state->scan;
  size_t index;

  for (index = 0; index < length && !state->done && !state->failed; index++)
    {
      char byte = data[index];

      if (!scan->chunked)
        {
          if (byte == '\n')
            {
              stream_feed_line (state, scan->line, scan->line_length);
              scan->line_length = 0;
            }
          else if (scan->line_length + 1 >= sizeof (scan->line))
            {
              print_formatted ("error: AI stream line exceeds limit\n");
              state->failed = 1;
              return;
            }
          else
            scan->line[scan->line_length++] = byte;
        }
      else if (scan->chunk_terminator > 0)
        {
          /* Swallow the CRLF that follows every chunk's data. */
          scan->chunk_terminator--;
          if (scan->chunk_terminator == 0)
            scan->chunk_header = 1;
        }
      else if (scan->chunk_header)
        {
          if (byte == '\n')
            {
              char *end;
              unsigned long chunk;

              errno = 0;
              scan->size[scan->size_length] = 0;
              chunk = strtoul (scan->size, &end, 16);
              if (errno == ERANGE || end == scan->size || chunk > MAX_RESPONSE)
                {
                  print_formatted ("error: AI stream chunk header is malformed\n");
                  state->failed = 1;
                  return;
                }
              scan->size_length = 0;
              scan->chunk_remaining = (size_t)chunk;
              scan->chunk_header = 0;
              if (chunk == 0)
                state->done = 1;
            }
          else if (scan->size_length + 1 >= sizeof (scan->size))
            {
              print_formatted ("error: AI stream chunk header is malformed\n");
              state->failed = 1;
              return;
            }
          else
            scan->size[scan->size_length++] = byte;
        }
      else
        {
          if (byte == '\n')
            {
              stream_feed_line (state, scan->line, scan->line_length);
              scan->line_length = 0;
            }
          else if (scan->line_length + 1 >= sizeof (scan->line))
            {
              print_formatted ("error: AI stream line exceeds limit\n");
              state->failed = 1;
              return;
            }
          else
            scan->line[scan->line_length++] = byte;
          if (scan->chunk_remaining > 0 && --scan->chunk_remaining == 0)
            scan->chunk_terminator = 2;
        }
    }
}

/* Main-thread only: write any newly accumulated deltas to stdout. */
static void
stream_flush_output (int final)
{
  size_t available = stream_available;
  size_t pending;

  if (stream_flushed >= available)
    pending = 0;
  else
    pending = available - stream_flushed;
  if (pending > 0)
    {
      if (!stream_printed_content)
        {
          print_formatted ("\n[AI Response]:\n");
          stream_printed_content = 1;
        }
      write_all (STDOUT_FILENO, stream_content + stream_flushed, pending);
      stream_flushed = available;
      output_partial_line = stream_content[stream_flushed - 1] != '\n';
    }
  /* The request thread and the main loop both finish the stream; emit the
     terminating newline only once. */
  if (final && stream_printed_content && !stream_terminated)
    {
      write_all (STDOUT_FILENO, "\n", 1);
      output_partial_line = 0;
      stream_terminated = 1;
    }
}

/* Reassemble the accumulated deltas into the buffered response shape. */
static int
stream_finish (StreamState *state, char *body, size_t capacity, size_t *response_length)
{
  Buffer buffer = { body, capacity > 0 ? capacity - 1 : 0, 0 };
  size_t index;
  char id_text[MAX_ID];
  int ok;

  ok = buffer_append_string (&buffer, "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"") >= 0
       && append_json_string_body (&buffer, stream_content, state->content_length) >= 0
       && buffer_append_string (&buffer, "\"") >= 0;
  if (ok && state->call_count > 0)
    {
      int emitted = 0;

      ok = buffer_append_string (&buffer, ",\"tool_calls\":[") >= 0;
      for (index = 0; ok && index < state->call_count; index++)
        {
          StreamToolCall *call = &state->calls[index];

          if (call->name[0] == 0)
            continue;
          if (call->id[0] != 0)
            snprintf (id_text, sizeof (id_text), "%s", call->id);
          else
            snprintf (id_text, sizeof (id_text), "tool-%zu", index);
          ok = (emitted == 0 || buffer_append_string (&buffer, ",") >= 0)
               && buffer_append_string (&buffer, "{\"id\":\"") >= 0
               && append_json_string_body (&buffer, id_text, strlen (id_text)) >= 0
               && buffer_append_string (&buffer, "\",\"type\":\"function\",\"function\":{\"name\":\"") >= 0
               && append_json_string_body (&buffer, call->name, call->name_length) >= 0
               && buffer_append_string (&buffer, "\",\"arguments\":\"") >= 0
               && append_json_string_body (&buffer, call->arguments, call->arguments_length) >= 0
               && buffer_append_string (&buffer, "\"}}") >= 0;
          emitted = 1;
        }
      if (ok)
        ok = buffer_append_string (&buffer, "]") >= 0;
    }
  if (ok)
    ok = buffer_append_string (&buffer, "}}]") >= 0;
  if (ok && state->have_usage)
    {
      char usage_text[160];

      snprintf (usage_text, sizeof (usage_text),
                ",\"usage\":{\"prompt_tokens\":%ld,\"completion_tokens\":%ld,\"total_tokens\":%ld}",
                state->prompt_tokens, state->completion_tokens, state->prompt_tokens + state->completion_tokens);
      ok = buffer_append_string (&buffer, usage_text) >= 0;
    }
  if (ok)
    ok = buffer_append_string (&buffer, "}") >= 0;
  if (!ok)
    return -1;
  body[buffer.length] = 0;
  *response_length = buffer.length;
  return 0;
}

static int
receive_stream (int socket_fd, char *response, size_t capacity, size_t total,
                size_t *response_length, char **body, int chunked, int *error_number)
{
  static StreamState state;
  time_t deadline;
  fd_set read_set;
  size_t body_offset = (size_t)(*body - response);

  memset (&state, 0, sizeof (state));
  state.scan.chunked = chunked;
  state.scan.chunk_header = chunked;
  stream_content[0] = 0;
  stream_available = 0;
  stream_flushed = 0;
  stream_terminated = 0;
  stream_active = 1;
  stream_feed_bytes (&state, *body, total - body_offset);
  /* A streaming reply can run long, so the timeout is per quiet period. */
  deadline = time (NULL) + api_timeout;
  while (!state.done && !state.failed && total < capacity - 1)
    {
      ssize_t received;
      int ready;
      time_t remaining;

      if (interrupted)
        {
          *error_number = EINTR;
          return -1;
        }
      FD_ZERO (&read_set);
      FD_SET (socket_fd, &read_set);
      remaining = deadline - time (NULL);
      if (remaining <= 0)
        {
          *error_number = ETIMEDOUT;
          return -3;
        }
      {
        struct timeval timeout = { remaining, 0 };

        ready = select (socket_fd + 1, &read_set, NULL, NULL, &timeout);
      }
      if (ready < 0)
        {
          if (errno == EINTR && !interrupted)
            continue;
          *error_number = errno;
          return -1;
        }
      if (ready == 0)
        {
          *error_number = ETIMEDOUT;
          return -3;
        }
      received = recv (socket_fd, response + total, capacity - 1 - total, 0);
      if (received < 0)
        {
          if (errno == EINTR && !interrupted)
            continue;
          *error_number = errno;
          return -1;
        }
      if (received == 0)
        break;
      total += (size_t)received;
      response[total] = 0;
      deadline = time (NULL) + api_timeout;
      stream_feed_bytes (&state, response + total - (size_t)received, (size_t)received);
    }
  stream_active = 0;
  if (state.failed)
    {
      /* An explicit error event, a limit overrun, or malformed SSE: the
         specific reason is already on screen. That is a definitive
         answer or a permanent fault, never a transient stumble — no
         retry (status -5, distinct from the empty-stream -4). */
      *error_number = EPROTO;
      return -5;
    }
  if (interrupted)
    {
      *error_number = EINTR;
      return -1;
    }
  if (!state.done && state.content_length == 0 && state.call_count == 0)
    {
      /* Distinct status for the receive path: a recovered retry shows
         only the retry notice, and the reason line moves to the give-up
         branch in post_endpoint_once. */
      *error_number = EPROTO;
      return -4;
    }
  if (stream_finish (&state, *body, capacity - body_offset, response_length) < 0)
    {
      print_formatted ("error: AI response exceeds conversation limit\n");
      *error_number = EOVERFLOW;
      return -1;
    }
  return 0;
}

static int
receive_response (int socket_fd, char *response, size_t capacity, size_t *response_length, char **body,
                  int *error_number, int *http_status, int *retry_after)
{
  time_t deadline;
  fd_set read_set;
  size_t total = 0;
  char *header_end = NULL;
  size_t content_length = 0;
  int have_content_length = 0;
  int chunked = 0;
  int event_stream = 0;
  int status = 0;
  char *line;

  *error_number = EIO;
  *http_status = 0;
  *retry_after = 0;
  deadline = time (NULL) + api_timeout;
  while (total < capacity - 1)
    {
      ssize_t received;
      int ready;
      time_t remaining;

      if (interrupted)
        {
          *error_number = EINTR;
          return -1;
        }
      FD_ZERO (&read_set);
      FD_SET (socket_fd, &read_set);
      remaining = deadline - time (NULL);
      if (remaining <= 0)
        {
          *error_number = ETIMEDOUT;
          return -3;
        }
      {
        struct timeval timeout = { remaining, 0 };

        ready = select (socket_fd + 1, &read_set, NULL, NULL, &timeout);
      }
      if (ready < 0)
        {
          if (errno == EINTR && !interrupted)
            continue;
          *error_number = errno;
          return -1;
        }
      if (ready == 0)
        {
          *error_number = ETIMEDOUT;
          return -3;
        }
      received = recv (socket_fd, response + total, capacity - 1 - total, 0);
      if (received < 0)
        {
          if (errno == EINTR && !interrupted)
            continue;
          *error_number = errno;
          return -1;
        }
      if (received == 0)
        break;
      total += (size_t)received;
      response[total] = 0;
      if (header_end == NULL)
        {
          header_end = strstr (response, "\r\n\r\n");
          if (header_end != NULL)
            {
              int code = 0;

              if (sscanf (response, "HTTP/%*d.%*d %d", &code) != 1)
                {
                  *error_number = EPROTO;
                  return -1;
                }
              status = code;
              *http_status = code;
              *body = header_end + 4;
            }
        }
      if (header_end != NULL)
        {
          line = response;
          while (line < header_end && line[0] != '\r')
            {
              char *next = memchr (line, '\r', (size_t)(header_end - line));
              size_t length;

              if (next == NULL)
                next = header_end;
              length = (size_t)(next - line);

              if (header_equal (line, length, "Content-Length"))
                {
                  char *end;
                  const char *value_start;
                  unsigned long value;
                  int conversion_error;

                  value_start = line + 15;
                  while (value_start < next && (*value_start == ' ' || *value_start == '\t'))
                    value_start++;
                  errno = 0;
                  value = strtoul (value_start, &end, 10);
                  conversion_error = errno == ERANGE;
                  if (conversion_error || end > next || end == value_start || value > MAX_RESPONSE)
                    {
                      *error_number = conversion_error ? ERANGE : EPROTO;
                      return -1;
                    }
                  content_length = (size_t)value;
                  have_content_length = 1;
                }
              else if (header_equal (line, length, "Transfer-Encoding") && length > 19
                       && strncasecmp (line + 18, " chunked", 8) == 0)
                chunked = 1;
              else if (header_equal (line, length, "Content-Type"))
                {
                  const char *value_start = line + 13;

                  while (value_start < next && (*value_start == ' ' || *value_start == '\t'))
                    value_start++;
                  if ((size_t)(next - value_start) >= 17
                      && strncasecmp (value_start, "text/event-stream", 17) == 0)
                    event_stream = 1;
                }
              else if (header_equal (line, length, "Retry-After"))
                {
                  char *end;
                  const char *value_start;
                  unsigned long value;

                  value_start = line + 12;
                  while (value_start < next && (*value_start == ' ' || *value_start == '\t'))
                    value_start++;
                  errno = 0;
                  value = strtoul (value_start, &end, 10);
                  /* Only the integer-seconds form is honored; an HTTP date
                     or junk is ignored and the exponential backoff applies. */
                  if (errno != ERANGE && end == next && value <= MAX_TIMEOUT)
                    *retry_after = (int)value;
                }
              line = next + 2;
            }
          if (event_stream && status >= 200 && status < 300)
            return receive_stream (socket_fd, response, capacity, total, response_length, body, chunked,
                                   error_number);
          if (have_content_length && total >= (size_t)(*body - response) + content_length)
            break;
          if (chunked && strstr (response, "\r\n0\r\n") != NULL)
            break;
        }
    }
  response[total] = 0;
  if (header_end == NULL)
    {
      *error_number = EPROTO;
      return -1;
    }
  if (status < 200 || status >= 300)
    {
      cJSON *payload = cJSON_Parse (*body);
      const cJSON *detail = NULL;

      if (payload != NULL)
        {
          const cJSON *error = cJSON_GetObjectItemCaseSensitive (payload, "error");

          if (cJSON_IsObject (error))
            detail = cJSON_GetObjectItemCaseSensitive (error, "message");
          if (!cJSON_IsString (detail))
            detail = cJSON_GetObjectItemCaseSensitive (payload, "message");
        }
      if (cJSON_IsString (detail))
        print_formatted ("error: AI endpoint returned HTTP %d: %.200s\n", status, cJSON_GetStringValue (detail));
      else
        print_formatted ("error: AI endpoint returned HTTP %d\n", status);
      cJSON_Delete (payload);
      return -2;
    }
  if (chunked)
    {
      char *original_body = *body;
      char *source = original_body;
      char *destination = original_body;

      while (source < response + total)
        {
          unsigned long chunk;
          char *next;
          int conversion_error;

          errno = 0;
          chunk = strtoul (source, &next, 16);
          conversion_error = errno == ERANGE;
          if (conversion_error || next == source || (*next != '\r' && *next != ';'))
            {
              *error_number = conversion_error ? ERANGE : EPROTO;
              return -1;
            }
          source = memchr (next, '\n', (size_t)(response + total - next));
          if (source == NULL)
            {
              *error_number = EPROTO;
              return -1;
            }
          source++;
          if (chunk == 0)
            break;
          if (chunk > (size_t)(response + total - source))
            {
              *error_number = EPROTO;
              return -1;
            }
          memmove (destination, source, chunk);
          destination += chunk;
          source += chunk;
          if (response + total - source < 2 || source[0] != '\r' || source[1] != '\n')
            {
              *error_number = EPROTO;
              return -1;
            }
          source += 2;
        }
      if (source > response + total)
        {
          *error_number = EPROTO;
          return -1;
        }
      *response_length = (size_t)(destination - original_body);
      *body = original_body;
    }
  else if (have_content_length)
    *response_length = content_length;
  else
    *response_length = total - (size_t)(*body - response);
  (*body)[*response_length] = 0;
  return 0;
}

static int
connect_endpoint (void)
{
  struct sockaddr_in address;
  int socket_fd;
  struct timeval timeout = { api_timeout, 0 };
  fd_set set;
  int result;
  int error;

  memset (&address, 0, sizeof (address));
  address.sin_family = AF_INET;
  address.sin_port = htons ((uint16_t)api_port);
  if (inet_pton (AF_INET, api_host, &address.sin_addr) != 1)
    {
      struct addrinfo hints;
      struct addrinfo *result;
      int status;

      memset (&hints, 0, sizeof (hints));
      hints.ai_family = AF_INET;
      hints.ai_socktype = SOCK_STREAM;
      status = getaddrinfo (api_host, NULL, &hints, &result);
      if (status != 0)
        {
          /* EAI_AGAIN is a resolver hiccup worth one retry; a name that
             does not exist cannot start existing within the backoff. */
          print_formatted ("error: resolve %s: %s\n", api_host, gai_strerror (status));
          resolve_transient = status == EAI_AGAIN;
          errno = ENOENT;
          return -2;
        }
      memcpy (&address.sin_addr, &((struct sockaddr_in *)result->ai_addr)->sin_addr,
              sizeof (address.sin_addr));
      freeaddrinfo (result);
    }
  socket_fd = socket (AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (socket_fd < 0)
    return -1;
  if (fcntl (socket_fd, F_SETFL, O_NONBLOCK) < 0)
    {
      close (socket_fd);
      return -1;
    }
  result = connect (socket_fd, (struct sockaddr *)&address, sizeof (address));
  if (result < 0)
    {
      if (errno != EINPROGRESS)
        {
          close (socket_fd);
          return -1;
        }
      FD_ZERO (&set);
      FD_SET (socket_fd, &set);
      result = select (socket_fd + 1, NULL, &set, NULL, &timeout);
      if (result <= 0)
        {
          close (socket_fd);
          errno = result == 0 ? ETIMEDOUT : EINTR;
          return -3;
        }
      socklen_t error_size = sizeof (error);

      error = 0;
      if (getsockopt (socket_fd, SOL_SOCKET, SO_ERROR, &error, &error_size) < 0 || error != 0)
        {
          close (socket_fd);
          errno = error != 0 ? error : EIO;
          return -1;
        }
    }
  if (fcntl (socket_fd, F_SETFL, 0) < 0)
    {
      close (socket_fd);
      return -1;
    }
  {
    struct timeval timeout = { api_timeout, 0 };

    if (setsockopt (socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof (timeout)) < 0)
      {
        close (socket_fd);
        return -1;
      }
  }
  return socket_fd;
}

static int
build_request (char *request, size_t capacity, size_t *length)
{
  Buffer buffer = { request, capacity, 0 };
  size_t index;

  if (buffer_append_string (&buffer, "{\"model\":") < 0 || append_json_text (&buffer, model) < 0
      || buffer_append_string (&buffer, ",\"messages\":[") < 0)
    return -1;
  for (index = 0; index < message_count; index++)
    {
      if (index != 0 && buffer_append_string (&buffer, ",") < 0)
        return -1;
      if (buffer_append (&buffer, messages[index].json, messages[index].length) < 0)
        return -1;
    }
  /* The compacted summary rides ahead of the plan so the folded turns
     outlive pruning, exactly like the plan itself. */
  if (summary_active)
    {
      char summary_json[2 * MAX_SUMMARY + 256];
      Buffer summary_buffer = { summary_json, sizeof (summary_json), 0 };

      if (buffer_append_string (&summary_buffer, "{\"role\":\"system\",\"content\":\"") < 0
          || append_json_string_body (&summary_buffer, summary_header, sizeof (summary_header) - 1) < 0
          || append_json_string_body (&summary_buffer, context_summary, strlen (context_summary)) < 0
          || buffer_append_string (&summary_buffer, "\"}") < 0)
        return -1;
      if (message_count != 0 && buffer_append_string (&buffer, ",") < 0)
        return -1;
      if (buffer_append (&buffer, summary_json, summary_buffer.length) < 0)
        return -1;
    }
  /* The plan rides as a fresh trailing system message so it never goes
     stale and survives context pruning of older turns. */
  if (plan_active)
    {
      char plan_json[2 * MAX_PLAN + 256];
      Buffer plan_buffer = { plan_json, sizeof (plan_json), 0 };

      if (buffer_append_string (&plan_buffer, "{\"role\":\"system\",\"content\":\"") < 0
          || append_json_string_body (&plan_buffer, plan_header, sizeof (plan_header) - 1) < 0
          || append_json_string_body (&plan_buffer, current_plan, strlen (current_plan)) < 0
          || buffer_append_string (&plan_buffer, "\"}") < 0)
        return -1;
      if (message_count != 0 && buffer_append_string (&buffer, ",") < 0)
        return -1;
      if (buffer_append (&buffer, plan_json, plan_buffer.length) < 0)
        return -1;
    }
  if (buffer_append_string (&buffer, "],\"tools\":[{\"type\":\"function\",\"function\":{"
                                     "\"name\":\"read_file\",\"description\":"
                                     "\"Read one text file inside the workspace and return numbered lines. "
                                     "Cheaper and safer than cat: no shell quoting and no approval needed.\","
                                     "\"parameters\":{\"type\":\"object\",\"properties\":{"
                                     "\"path\":{\"type\":\"string\",\"description\":"
                                     "\"File path relative to the workspace, or absolute inside it.\"},"
                                     "\"offset\":{\"type\":\"integer\",\"description\":"
                                     "\"First line to return, 1-based; default 1.\"},"
                                     "\"limit\":{\"type\":\"integer\",\"description\":"
                                     "\"Maximum number of lines to return; default unlimited.\"}},"
                                     "\"required\":[\"path\"]}}},"
                                     "{\"type\":\"function\",\"function\":{"
                                     "\"name\":\"list_dir\",\"description\":"
                                     "\"List one directory inside the workspace: sorted entries with type and size. "
                                     "Cheaper and safer than ls: no approval needed.\","
                                     "\"parameters\":{\"type\":\"object\",\"properties\":{"
                                     "\"path\":{\"type\":\"string\",\"description\":"
                                     "\"Directory path relative to the workspace, or absolute inside it.\"}},"
                                     "\"required\":[\"path\"]}}},"
                                     "{\"type\":\"function\",\"function\":{"
                                     "\"name\":\"apply_patch\",\"description\":"
                                     "\"Edit files with whole-line SEARCH/REPLACE hunks. Provide the file path on its "
                                     "own line, then blocks of '<<<<<<< SEARCH', the exact existing lines, '=======', "
                                     "the replacement lines, '>>>>>>> REPLACE'. SEARCH must match exactly one "
                                     "location; an empty SEARCH block creates a new file; an empty REPLACE block "
                                     "deletes the matched lines. Several hunks and several files per call are fine.\","
                                     "\"parameters\":{\"type\":\"object\",\"properties\":{"
                                     "\"patch\":{\"type\":\"string\",\"description\":"
                                     "\"The full SEARCH/REPLACE patch text.\"}},"
                                     "\"required\":[\"patch\"]}}},"
                                     "{\"type\":\"function\",\"function\":{"
                                     "\"name\":\"plan\",\"description\":"
                                     "\"Record the full current task checklist. The latest plan is pinned into every "
                                     "later request, so it survives context pruning. Send the complete checklist each "
                                     "time, not a delta; an empty checklist clears the plan.\","
                                     "\"parameters\":{\"type\":\"object\",\"properties\":{"
                                     "\"checklist\":{\"type\":\"string\",\"description\":"
                                     "\"The complete checklist text.\"}},"
                                     "\"required\":[\"checklist\"]}}}")
      < 0)
    return -1;
  /* The shell-touching tools disappear under --no-shell, leaving the
     read-only analyst set. */
  if (!no_shell
      && buffer_append_string (&buffer, ",{\"type\":\"function\",\"function\":{"
                                        "\"name\":\"run_terminal_command\",\"description\":"
                                        "\"Execute one shell command on the user's host and return bounded live output.\","
                                        "\"parameters\":{\"type\":\"object\",\"properties\":{"
                                        "\"command\":{\"type\":\"string\",\"description\":"
                                        "\"The complete shell command to execute.\"}},"
                                        "\"required\":[\"command\"]}}},"
                                        "{\"type\":\"function\",\"function\":{"
                                        "\"name\":\"start_job\",\"description\":"
                                        "\"Start one long-running command (test suite, build) as a background job "
                                        "and return immediately as job:N started. Output appends to the job log; "
                                        "poll with job_status. Jobs are exempt from the command timeout and keep "
                                        "the same approval and workspace rules as foreground commands.\","
                                        "\"parameters\":{\"type\":\"object\",\"properties\":{"
                                        "\"command\":{\"type\":\"string\",\"description\":"
                                        "\"The complete shell command to run in the background.\"}},"
                                        "\"required\":[\"command\"]}}},"
                                        "{\"type\":\"function\",\"function\":{"
                                        "\"name\":\"job_status\",\"description\":"
                                        "\"Report background jobs: with no id a one-line-per-job summary, with id "
                                        "the state, elapsed time, and the bounded tail of its log. Read-only, no "
                                        "approval needed.\","
                                        "\"parameters\":{\"type\":\"object\",\"properties\":{"
                                        "\"id\":{\"type\":\"integer\",\"description\":"
                                        "\"Job id returned by start_job; omit to list all jobs.\"}}}}},"
                                        "{\"type\":\"function\",\"function\":{"
                                        "\"name\":\"stop_job\",\"description\":"
                                        "\"Stop one background job: SIGTERM to its process group, SIGKILL after a "
                                        "grace second. Needs approval, like running a command.\","
                                        "\"parameters\":{\"type\":\"object\",\"properties\":{"
                                        "\"id\":{\"type\":\"integer\",\"description\":"
                                        "\"Job id returned by start_job.\"}},"
                                        "\"required\":[\"id\"]}}}")
      < 0)
    return -1;
  /* Delegation never nests: subagents do not see the task tool. */
  if (!subagent_depth
      && buffer_append_string (&buffer, ",{\"type\":\"function\",\"function\":{"
                                        "\"name\":\"task\",\"description\":"
                                        "\"Delegate one self-contained subtask to a fresh nested agent with its own "
                                        "empty context. Write the complete brief: goal, constraints, and exactly "
                                        "what to report back; the subagent cannot see this conversation and its "
                                        "whole output tail becomes the tool result. Depth is limited to one level "
                                        "and a delegation costs a full agent run.\","
                                        "\"parameters\":{\"type\":\"object\",\"properties\":{"
                                        "\"prompt\":{\"type\":\"string\",\"description\":"
                                        "\"The complete, self-contained subtask prompt.\"}},"
                                        "\"required\":[\"prompt\"]}}}")
      < 0)
    return -1;
  if (buffer_append_string (&buffer, "],\"tool_choice\":\"auto\"") < 0)
    return -1;
  if (buffer_append_string (&buffer, streaming_enabled ? ",\"stream\":true}" : "}") < 0)
    return -1;
  *length = buffer.length;
  return 0;
}

static int
transient_network_error (int error)
{
  return error == ECONNRESET || error == ECONNABORTED || error == EPIPE || error == ECONNREFUSED
         || error == EHOSTUNREACH || error == ENETUNREACH;
}

static int
post_endpoint_once (const char *request, size_t request_length, const char **response_body, char *response_storage,
                    int *transient, int *retry_after, int final_attempt)
{
  char headers[1024];
  int header_length;
  int socket_fd;
  int receive_status;
  int receive_error;
  int http_status;
  size_t response_length;
  char *body;

  *transient = 0;
  *retry_after = 0;
  response_length = 0;
  body = NULL;
  stream_printed_content = 0;
  stream_active = 0;
  resolve_transient = 0;
  if (api_key[0] != 0)
    header_length = snprintf (headers, sizeof (headers),
                              "POST %s HTTP/1.1\r\nHost: %s:%d\r\n"
                              "Authorization: Bearer %s\r\nContent-Type: application/json\r\n"
                              "Content-Length: %zu\r\nConnection: close\r\n\r\n",
                              api_path, api_host, api_port, api_key, request_length);
  else
    header_length = snprintf (headers, sizeof (headers),
                              "POST %s HTTP/1.1\r\nHost: %s:%d\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: %zu\r\nConnection: close\r\n\r\n",
                              api_path, api_host, api_port, request_length);
  if (header_length < 0 || (size_t)header_length >= sizeof (headers))
    return -1;
  socket_fd = connect_endpoint ();
  if (socket_fd < 0)
    {
      /* A resolve failure already printed its own line. */
      if (socket_fd != -2)
        print_formatted ("error: connect %s:%d: %s\n", api_host, api_port, strerror (errno));
      if (errno == ETIMEDOUT)
        {
          agent_exit_status = EXIT_TIMEOUT;
          return -EXIT_TIMEOUT;
        }
      if (resolve_transient || transient_network_error (errno))
        *transient = 1;
      return -1;
    }
  if (write_all (socket_fd, headers, (size_t)header_length) < 0 || write_all (socket_fd, request, request_length) < 0)
    {
      print_formatted ("error: send request: %s\n", strerror (errno));
      close (socket_fd);
      if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT)
        {
          agent_exit_status = EXIT_TIMEOUT;
          return -EXIT_TIMEOUT;
        }
      if (transient_network_error (errno))
        *transient = 1;
      return -1;
    }
  receive_status = receive_response (socket_fd, response_storage, MAX_RESPONSE, &response_length, &body,
                                     &receive_error, &http_status, retry_after);
  close (socket_fd);
  if (receive_status == -2)
    {
      if (http_status == 429 || http_status >= 500)
        *transient = 1;
      return -1;
    }
  if (receive_status == -5)
    {
      /* The stream consumer printed the specific reason; a definitive
         answer or permanent fault needs neither a retry nor a second
         line. */
      return -1;
    }
  if (receive_status < 0)
    {
      /* EPROTO means the response arrived empty or truncated: an empty
         event stream, cut-off headers, or broken framing (-4 is the
         empty-stream form). Nothing was shown on screen yet, so
         replaying the request is safe and a transient flake gets the
         retry treatment. Recovered attempts print only the retry
         notice; the reason line waits for the final failure. */
      int retryable = !interrupted
                      && (transient_network_error (receive_error)
                          || (receive_error == EPROTO && !stream_printed_content));

      if (interrupted)
        {
          print_line_break ();
          print_formatted ("[request interrupted]\n");
        }
      else if (!interrupted && (!retryable || final_attempt))
        {
          if (receive_status == -4)
            print_formatted ("error: AI stream ended without content\n");
          else
            print_formatted ("error: receive response: %s\n", strerror (receive_error));
        }
      if (receive_status == -3)
        agent_exit_status = EXIT_TIMEOUT;
      else if (retryable)
        *transient = 1;
      /* Never replay a request whose output is already on screen. */
      if (stream_printed_content)
        *transient = 0;
      return agent_exit_status == EXIT_SUCCESS ? -1 : -agent_exit_status;
    }
  (void)response_length;
  *response_body = body;
  return 0;
}

static int
call_endpoint_once (const char **response_body, char *response_storage, int *transient, int *retry_after,
                    int final_attempt)
{
  static char request[MAX_REQUEST];
  size_t request_length;

  if (build_request (request, sizeof (request), &request_length) < 0)
    {
      print_formatted ("error: conversation exceeds the request limit\n");
      return -1;
    }
  return post_endpoint_once (request, request_length, response_body, response_storage, transient, retry_after,
                             final_attempt);
}

/* Sleep in slices so Ctrl+C and the interrupted flag stay responsive. */
static int
sleep_slices (int seconds)
{
  time_t deadline = time (NULL) + seconds;

  while (time (NULL) < deadline)
    {
      if (interrupted)
        return -1;
      usleep (100 * 1000);
    }
  return 0;
}

static int
call_endpoint (const char **response_body, char *response_storage)
{
  int attempt;

  for (attempt = 1;; attempt++)
    {
      int transient = 0;
      int retry_after = 0;
      /* The last allowed attempt must surface the failure reason itself:
         no retry notice will follow it. */
      int final_attempt = attempt >= MAX_HTTP_ATTEMPTS;
      int result = call_endpoint_once (response_body, response_storage, &transient, &retry_after, final_attempt);
      int delay;

      if (result == 0 || !transient || attempt >= MAX_HTTP_ATTEMPTS || interrupted)
        {
          stream_flush_output (result == 0);
          return result;
        }
      delay = retry_after > 0 ? retry_after : 1 << (attempt - 1);
      if (delay > MAX_RETRY_AFTER)
        delay = MAX_RETRY_AFTER;
      print_line_break ();
      print_formatted ("[retrying in %ds (attempt %d/%d)]\n", delay, attempt + 1, MAX_HTTP_ATTEMPTS);
      if (sleep_slices (delay) < 0)
        return result;
    }
}

typedef struct
{
  int result;
  const char *response_body;
  char *response_storage;
} EndpointRequest;

typedef struct
{
  pthread_t worker;
  pthread_mutex_t mutex;
  pthread_cond_t completed_cond;
  int done;
  EndpointRequest request;
} EndpointWait;

static void *
run_endpoint_request (void *argument)
{
  EndpointWait *wait = argument;

  wait->request.result = call_endpoint (&wait->request.response_body, wait->request.response_storage);
  pthread_mutex_lock (&wait->mutex);
  wait->done = 1;
  pthread_cond_broadcast (&wait->completed_cond);
  pthread_mutex_unlock (&wait->mutex);
  return NULL;
}

static int
call_endpoint_threaded (const char **response_body, char *response_storage)
{
  EndpointWait wait;
  int create_error;
  void *thread_result;

  memset (&wait, 0, sizeof (wait));
  wait.request.response_storage = response_storage;
  pthread_mutex_init (&wait.mutex, NULL);
  pthread_cond_init (&wait.completed_cond, NULL);
  create_error = pthread_create (&wait.worker, NULL, run_endpoint_request, &wait);
  if (create_error != 0)
    {
      errno = create_error;
      print_formatted ("error: create AI request thread: %s\n", strerror (errno));
      pthread_cond_destroy (&wait.completed_cond);
      pthread_mutex_destroy (&wait.mutex);
      return call_endpoint (response_body, response_storage);
    }
  begin_live_input ();
  if (ui_mode || !one_shot)
    live_steering_prompt ();
  for (;;)
    {
      int worker_done;

      pthread_mutex_lock (&wait.mutex);
      worker_done = wait.done;
      pthread_mutex_unlock (&wait.mutex);
      if (worker_done || interrupted)
        break;
      poll_live_input ();
      if (stream_active)
        stream_flush_output (0);
      usleep (100 * 1000);
    }
  poll_live_input ();
  end_live_input ();
  if (!wait.done)
    {
      pthread_cancel (wait.worker);
      pthread_join (wait.worker, &thread_result);
      stream_flush_output (0);
      print_line_break ();
      print_formatted ("[AI request inactive]\n");
      print_formatted ("[live steering inactive]\n");
      print_formatted ("[request interrupted]\n");
      pthread_cond_destroy (&wait.completed_cond);
      pthread_mutex_destroy (&wait.mutex);
      return -agent_exit_status;
    }
  pthread_join (wait.worker, &thread_result);
  /* Finish the response before the markers so a partial tail cannot glue
     onto them or land after them. */
  stream_flush_output (wait.request.result == 0);
  print_line_break ();
  print_formatted ("[AI request inactive]\n");
  print_formatted ("[live steering inactive]\n");
  if (wait.request.result == 0)
    *response_body = wait.request.response_body;
  create_error = wait.request.result;
  pthread_cond_destroy (&wait.completed_cond);
  pthread_mutex_destroy (&wait.mutex);
  return create_error;
}

static int
read_context_chunk (FILE *source, Buffer *buffer, const char *description, size_t *context_length)
{
  char chunk[4096];
  size_t read_length;
  size_t total = 0;

  while ((read_length = fread (chunk, 1, sizeof (chunk), source)) != 0)
    {
      if (memchr (chunk, 0, read_length) != NULL)
        {
          print_formatted ("error: %s contains a NUL byte\n", description);
          return -1;
        }
      total += read_length;
      if (*context_length + total > MAX_PROMPT_CONTEXT)
        {
          print_formatted ("error: one-shot context exceeds %d bytes\n", MAX_PROMPT_CONTEXT);
          return -1;
        }
      if (append_xml_escaped (buffer, chunk, read_length) < 0)
        return -1;
    }
  if (ferror (source))
    {
      print_formatted ("error: cannot read %s\n", description);
      return -1;
    }
  return 0;
}

static int
append_context_header (Buffer *buffer, const char *tag, const char *path)
{
  if (buffer_append_string (buffer, "<") < 0 || buffer_append_string (buffer, tag) < 0)
    return -1;
  if (path != NULL)
    {
      if (buffer_append_string (buffer, " path=\"") < 0 || append_xml_text (buffer, path) < 0
          || buffer_append_string (buffer, "\"") < 0)
        return -1;
    }
  return buffer_append_string (buffer, ">\n");
}

static int
append_context_file (Buffer *buffer, const char *path, size_t *context_length)
{
  FILE *source;

  if (strcmp (path, "-") == 0)
    {
      print_formatted ("error: '-' is not supported for -f; pipe stdin instead\n");
      return -1;
    }
  source = fopen (path, "rb");
  if (source == NULL)
    {
      print_formatted ("error: cannot open %s: %s\n", path, strerror (errno));
      return -1;
    }
  if (append_context_header (buffer, "file", path) < 0 || read_context_chunk (source, buffer, path, context_length) < 0
      || buffer_append_string (buffer, "\n</file>\n\n") < 0)
    {
      fclose (source);
      return -1;
    }
  fclose (source);
  return 0;
}

static int
append_piped_context (Buffer *buffer, size_t *context_length)
{
  int first;

  if (!one_shot || isatty (STDIN_FILENO))
    return 0;
  first = fgetc (stdin);
  if (first == EOF)
    return 0;
  ungetc (first, stdin);
  if (append_context_header (buffer, "stdin", NULL) < 0
      || read_context_chunk (stdin, buffer, "piped stdin", context_length) < 0
      || buffer_append_string (buffer, "\n</stdin>\n\n") < 0)
    return -1;
  return 0;
}

static int
append_attachment_context (Buffer *buffer)
{
  size_t context_length = 0;
  int index;

  for (index = 0; index < attachment_count; index++)
    if (append_context_file (buffer, attachment_paths[index], &context_length) < 0)
      return -1;
  return 0;
}

static int
build_one_shot_prompt (const char *prompt, Buffer *buffer)
{
  size_t context_length = 0;

  if (append_attachment_context (buffer) < 0)
    return -1;
  if (append_piped_context (buffer, &context_length) < 0)
    return -1;
  return buffer_append_string (buffer, prompt);
}

static int
build_prompt_with_attachments (const char *prompt, Buffer *buffer)
{
  if (append_attachment_context (buffer) < 0)
    return -1;
  return buffer_append_string (buffer, prompt);
}

static int
read_line (const char *prompt, char *buffer, size_t capacity)
{
  size_t length;
  int character;

  if (prompt != NULL && prompt[0] != 0)
    print_formatted ("%s\n", prompt);
  if (fgets (buffer, (int)capacity, stdin) == NULL)
    return -1;
  length = strlen (buffer);
  if (length > 0 && buffer[length - 1] == '\n')
    buffer[length - 1] = 0;
  else
    {
      while ((character = getchar ()) != EOF && character != '\n')
        ;
    }
  return 0;
}

static int
multiline_keys_active (void)
{
  /* The TUI drives ai through a pipe and piped prompts keep the
     empty-line convention; only a real terminal can tell C-J (newline)
     apart from Enter (submit). */
  return isatty (STDIN_FILENO);
}

static int
read_multiline (const char *prompt, const char *continuation_prompt, char *buffer, size_t capacity)
{
  char line[MAX_LINE];
  size_t length = 0;

  if (multiline_keys_active ())
    return editor_read (prompt, continuation_prompt, buffer, capacity);
  buffer[0] = 0;
  for (;;)
    {
      if (read_line (length == 0 ? prompt : continuation_prompt, line, sizeof (line)) < 0)
        return -1;
      if (line[0] == 0)
        break;
      if (length + strlen (line) + 2 >= capacity)
        {
          print_formatted ("error: input is too long\n");
          return -1;
        }
      if (length != 0)
        buffer[length++] = '\n';
      memcpy (buffer + length, line, strlen (line) + 1);
      length += strlen (line);
    }
  return 0;
}

static int
append_tool_result (const ToolCall *call, const char *content)
{
  char json[MESSAGE_SIZE];
  Buffer buffer = { json, sizeof (json), 0 };

  if (buffer_append_string (&buffer, "{\"role\":\"tool\",\"tool_call_id\":") < 0
      || append_json_text (&buffer, call->id) < 0 || buffer_append_string (&buffer, ",\"name\":") < 0
      || append_json_text (&buffer, call->name) < 0 || buffer_append_string (&buffer, ",\"content\":") < 0
      || append_json_text (&buffer, content) < 0 || buffer_append_string (&buffer, "}") < 0)
    return -1;
  return add_message (json, buffer.length, ROLE_TOOL);
}

static int
append_tool_output (const ToolCall *call, const char *prefix, const char *output, size_t output_length)
{
  char content[MODEL_OUTPUT_SIZE + MAX_LINE];
  char json[MESSAGE_SIZE];
  Buffer buffer = { json, sizeof (json), 0 };
  size_t content_length;

  content_length = strlen (prefix);
  if (content_length >= sizeof (content) || output_length > sizeof (content) - content_length - 1)
    return -1;
  memcpy (content, prefix, content_length);
  memcpy (content + content_length, output, output_length);
  content[content_length + output_length] = 0;

  if (buffer_append_string (&buffer, "{\"role\":\"tool\",\"tool_call_id\":") < 0
      || append_json_text (&buffer, call->id) < 0 || buffer_append_string (&buffer, ",\"name\":") < 0
      || append_json_text (&buffer, call->name) < 0 || buffer_append_string (&buffer, ",\"content\":") < 0
      || append_json_string (&buffer, content, content_length + output_length) < 0
      || buffer_append_string (&buffer, "}") < 0)
    return -1;
  return add_message (json, buffer.length, ROLE_TOOL);
}

static int
append_tool_call_arguments (Buffer *buffer, const ToolCall *call)
{
  static char arguments[6 * MAX_PATCH + 96];
  const char *key;
  const char *value;
  Buffer inner = { arguments, sizeof (arguments), 0 };

  /* Re-encode the argument keys of the called built-in tool. The arguments
     are a JSON string nested inside the message JSON, so each value is
     escaped for the argument object and the whole argument text is escaped
     again for the enclosing message string. */
  if (call->kind == TOOL_PLAN)
    {
      key = "checklist";
      value = call->checklist;
    }
  else if (call->kind == TOOL_READ_FILE || call->kind == TOOL_LIST_DIR)
    {
      key = "path";
      value = call->path;
    }
  else if (call->kind == TOOL_APPLY_PATCH)
    {
      key = "patch";
      value = call->patch;
    }
  else
    {
      key = "command";
      value = call->command;
    }
  if (buffer_append_string (&inner, "{\"") < 0 || buffer_append_string (&inner, key) < 0
      || buffer_append_string (&inner, "\":\"") < 0
      || append_json_string_body (&inner, value, strlen (value)) < 0
      || buffer_append_string (&inner, "\"") < 0)
    return -1;
  if (call->kind == TOOL_READ_FILE)
    {
      char optionals[96];

      if (snprintf (optionals, sizeof (optionals), ",\"offset\":%ld,\"limit\":%ld", call->offset, call->limit)
          >= (int)sizeof (optionals))
        return -1;
      if (buffer_append_string (&inner, optionals) < 0)
        return -1;
    }
  if (buffer_append_string (&inner, "}") < 0)
    return -1;
  return buffer_append_string (buffer, "\"") < 0
         || append_json_string_body (buffer, arguments, inner.length) < 0
         || buffer_append_string (buffer, "\"") < 0;
}

static int
add_assistant_response (const char *content, const ToolCall *calls, size_t call_count)
{
  char json[MESSAGE_SIZE];
  Buffer buffer = { json, sizeof (json), 0 };
  size_t index;

  if (buffer_append_string (&buffer, "{\"role\":\"assistant\",\"content\":") < 0
      || (content[0] != 0 && append_json_text (&buffer, content) < 0)
      || (content[0] == 0 && buffer_append_string (&buffer, "null") < 0))
    return -1;
  if (call_count != 0)
    {
      if (buffer_append_string (&buffer, ",\"tool_calls\":[") < 0)
        return -1;
      for (index = 0; index < call_count; index++)
        {
          if (index != 0 && buffer_append_string (&buffer, ",") < 0)
            return -1;
          if (buffer_append_string (&buffer, "{\"type\":\"function\",\"id\":") < 0
              || append_json_text (&buffer, calls[index].id) < 0
              || buffer_append_string (&buffer, ",\"function\":{\"name\":") < 0
              || append_json_text (&buffer, calls[index].name) < 0
              || buffer_append_string (&buffer, ",\"arguments\":") < 0
              || append_tool_call_arguments (&buffer, &calls[index]) < 0
              || buffer_append_string (&buffer, "}}") < 0)
            return -1;
        }
      if (buffer_append_string (&buffer, "]") < 0)
        return -1;
    }
  if (buffer_append_string (&buffer, "}") < 0)
    return -1;
  return add_message (json, buffer.length, call_count != 0 ? ROLE_ASSISTANT_TOOLS : ROLE_ASSISTANT);
}

/* Echo one chunk to the terminal and keep its tail in the capture
   buffer, sliding earlier bytes out once it fills. */
static int
capture_chunk (const char *chunk, ssize_t received, char *output, size_t capacity, size_t *captured,
               size_t *dropped)
{
  size_t amount = (size_t)received;

  if (write_all (STDOUT_FILENO, chunk, (size_t)received) < 0)
    return -1;
  output_partial_line = chunk[received - 1] != '\n';
  /* Keep the tail: once the buffer fills, slide it so the model sees the
     end of the output, where errors usually are. A single read never
     exceeds sizeof(chunk), far below capacity. */
  if (amount > capacity)
    amount = capacity;
  if (*captured + amount > capacity)
    {
      size_t shift = *captured + amount - capacity;

      *dropped += shift;
      memmove (output, output + shift, *captured - shift);
      *captured -= shift;
    }
  memcpy (output + *captured, chunk, amount);
  *captured += amount;
  return 0;
}

static int
run_command (const char *command, char *output, size_t capacity, size_t *output_length, size_t *dropped_length,
             int *exit_status, int *timed_out)
{
  int pipe_fds[2];
  pid_t child;
  time_t deadline = command_timeout > 0 ? time (NULL) + command_timeout : 0;
  size_t captured = 0;
  size_t dropped = 0;
  int status = 0;
  int have_status = 0;
  struct pollfd descriptor;

  *timed_out = 0;

  if (pipe2 (pipe_fds, O_CLOEXEC) < 0)
    return -1;
  /* Only the parent's read end polls; the child's dup2'd stdout stays
     blocking, so a full pipe applies backpressure instead of failing
     the command's writes mid-stream. */
  if (fcntl (pipe_fds[0], F_SETFL, O_NONBLOCK) < 0)
    {
      close (pipe_fds[0]);
      close (pipe_fds[1]);
      return -1;
    }
  interrupted = 0;
  child = fork ();
  if (child < 0)
    {
      close (pipe_fds[0]);
      close (pipe_fds[1]);
      return -1;
    }
  if (child == 0)
    {
      struct sigaction action;

      memset (&action, 0, sizeof (action));
      action.sa_handler = SIG_DFL;
      sigaction (SIGINT, &action, NULL);
      sigaction (SIGTERM, &action, NULL);
      setpgid (0, 0);
      close (pipe_fds[0]);
      dup2 (pipe_fds[1], STDOUT_FILENO);
      dup2 (pipe_fds[1], STDERR_FILENO);
      if (pipe_fds[1] != STDOUT_FILENO && pipe_fds[1] != STDERR_FILENO)
        close (pipe_fds[1]);
      /* The lexical filter only inspected the command string; Landlock
         bounds what the executed tree can actually touch. A failure here
         already printed a warning and the command still runs scoped by
         the lexical filter alone. Shell-mode commands skip both filters:
         the human typed them, exactly like --full-access. */
      if (workspace_directory != NULL && !full_access && !shell_direct && landlock_enabled)
        landlock_apply (workspace_root);
      execl ("/bin/sh", "sh", "-c", command, (char *)NULL);
      _exit (127);
    }
  setpgid (child, child);
  close (pipe_fds[1]);
  descriptor.fd = pipe_fds[0];
  descriptor.events = POLLIN;
  begin_live_input ();
  print_formatted ("[live steering active]\n");
  print_formatted ("[command output]\n");
  for (;;)
    {
      char chunk[2048];
      ssize_t received;
      int poll_result;

      if (interrupted)
        kill (-child, SIGINT);
      poll_result = poll (&descriptor, 1, 100);
      if (poll_result < 0)
        {
          if (errno == EINTR)
            continue;
          break;
        }
      poll_live_input ();
      if (poll_result > 0 && (descriptor.revents & (POLLIN | POLLHUP)))
        {
          received = read (pipe_fds[0], chunk, sizeof (chunk));
          if (received > 0)
            {
              if (capture_chunk (chunk, received, output, capacity, &captured, &dropped) < 0)
                break;
            }
          else if (received == 0)
            break;
          else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            break;
        }
      if (waitpid (child, &status, WNOHANG) == child)
        {
          /* The child is gone, but its last writes can still be buffered
             in the pipe; drain them so the captured tail keeps the true
             end of the output. The pipe is non-blocking, and a detached
             descendant may keep the write end open, so stop at the first
             dry read instead of waiting for EOF — and cap the effort, so
             a descendant that floods the pipe cannot pin the loop. */
          int rounds = 256;

          while (rounds-- > 0)
            {
              received = read (pipe_fds[0], chunk, sizeof (chunk));
              if (received < 0 && errno == EINTR)
                continue;
              if (received <= 0)
                break;
              if (capture_chunk (chunk, received, output, capacity, &captured, &dropped) < 0)
                break;
            }
          have_status = 1;
          break;
        }
      if (command_timeout > 0 && time (NULL) >= deadline)
        {
          kill (-child, SIGTERM);
          sleep (1);
          kill (-child, SIGKILL);
          waitpid (child, &status, 0);
          have_status = 1;
          *timed_out = 1;
          print_line_break ();
          print_formatted ("[command timed out after %d seconds]\n", command_timeout);
          agent_exit_status = EXIT_TIMEOUT;
          break;
        }
    }
  while (!have_status && waitpid (child, &status, 0) < 0 && errno == EINTR)
    ;
  end_live_input ();
  print_line_break ();
  print_formatted ("[live steering inactive]\n");
  close (pipe_fds[0]);
  if (WIFEXITED (status))
    *exit_status = WEXITSTATUS (status);
  else if (WIFSIGNALED (status))
    *exit_status = 128 + WTERMSIG (status);
  else
    *exit_status = 1;
  *output_length = captured;
  *dropped_length = dropped;
  output[captured] = 0;
  return 0;
}

/* Resolve this executable once, so a task tool can re-exec the same static
   binary as a fresh subagent. */
static const char *
self_executable (void)
{
  static char path[PATH_MAX];
  static int resolved;

  if (!resolved)
    {
      ssize_t length = readlink ("/proc/self/exe", path, sizeof (path) - 1);

      if (length > 0)
        path[length] = 0;
      else
        path[0] = 0;
      resolved = 1;
    }
  return path[0] != 0 ? path : NULL;
}

/* Subagent delegation: fork and re-exec this binary with the task prompt as
   a one-shot run. The child gets an empty context and the inherited AI_*
   environment plus AI_SUBAGENT=1 (no task tool, no transcript). Its stdout
   is captured tail-bounded like command output and also streamed live, so
   the human watches the run and — in terminal mode — sees the child's own
   approval prompts, which it reads from /dev/tty. Under the TUI no safe
   approval routing exists, so the child is dropped to --no-shell unless
   full access was granted. Unlike run_command, a timeout never sets the
   parent's exit status: a slow subtask is the model's problem, not a
   session failure. */
static int
task_run (const char *prompt, char *output, size_t capacity, size_t *output_length, size_t *dropped_length,
          int *exit_status, int *timed_out)
{
  const char *self = self_executable ();
  int pipe_fds[2];
  int null_fd;
  pid_t child;
  time_t deadline = command_timeout > 0 ? time (NULL) + command_timeout : 0;
  size_t captured = 0;
  size_t dropped = 0;
  int status = 0;
  int have_status = 0;
  struct pollfd descriptor;

  *timed_out = 0;
  if (self == NULL)
    return -1;
  if (pipe2 (pipe_fds, O_CLOEXEC) < 0)
    return -1;
  if (fcntl (pipe_fds[0], F_SETFL, O_NONBLOCK) < 0)
    {
      close (pipe_fds[0]);
      close (pipe_fds[1]);
      return -1;
    }
  interrupted = 0;
  child = fork ();
  if (child < 0)
    {
      close (pipe_fds[0]);
      close (pipe_fds[1]);
      return -1;
    }
  if (child == 0)
    {
      char *arguments[8];
      int count = 0;

      arguments[count++] = (char *) self;
      if (workspace_directory != NULL)
        {
          arguments[count++] = (char *) "-C";
          arguments[count++] = workspace_root;
        }
      if (full_access)
        arguments[count++] = (char *) "--full-access";
      else if (ui_mode)
        arguments[count++] = (char *) "--no-shell";
      arguments[count++] = (char *) "-exec";
      arguments[count++] = (char *) prompt;
      arguments[count] = NULL;
      setpgid (0, 0);
      null_fd = open ("/dev/null", O_RDONLY);
      if (null_fd >= 0)
        {
          dup2 (null_fd, STDIN_FILENO);
          if (null_fd != STDIN_FILENO)
            close (null_fd);
        }
      close (pipe_fds[0]);
      dup2 (pipe_fds[1], STDOUT_FILENO);
      dup2 (pipe_fds[1], STDERR_FILENO);
      if (pipe_fds[1] != STDOUT_FILENO && pipe_fds[1] != STDERR_FILENO)
        close (pipe_fds[1]);
      setenv ("AI_SUBAGENT", "1", 1);
      execv (self, arguments);
      _exit (127);
    }
  setpgid (child, child);
  close (pipe_fds[1]);
  descriptor.fd = pipe_fds[0];
  descriptor.events = POLLIN;
  print_formatted ("[task output]\n");
  for (;;)
    {
      char chunk[2048];
      ssize_t received;
      int poll_result;

      if (interrupted)
        kill (-child, SIGINT);
      poll_result = poll (&descriptor, 1, 100);
      if (poll_result < 0)
        {
          if (errno == EINTR)
            continue;
          break;
        }
      if (poll_result > 0 && (descriptor.revents & (POLLIN | POLLHUP)))
        {
          received = read (pipe_fds[0], chunk, sizeof (chunk));
          if (received > 0)
            {
              write_all (STDOUT_FILENO, chunk, (size_t) received);
              if (capture_chunk (chunk, received, output, capacity, &captured, &dropped) < 0)
                break;
            }
          else if (received == 0)
            break;
          else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            break;
        }
      if (waitpid (child, &status, WNOHANG) == child)
        {
          /* Drain buffered last writes, like run_command's reaped pipe. */
          int rounds = 256;

          while (rounds-- > 0)
            {
              received = read (pipe_fds[0], chunk, sizeof (chunk));
              if (received < 0 && errno == EINTR)
                continue;
              if (received <= 0)
                break;
              write_all (STDOUT_FILENO, chunk, (size_t) received);
              if (capture_chunk (chunk, received, output, capacity, &captured, &dropped) < 0)
                break;
            }
          have_status = 1;
          break;
        }
      if (command_timeout > 0 && time (NULL) >= deadline)
        {
          kill (-child, SIGTERM);
          sleep (1);
          kill (-child, SIGKILL);
          waitpid (child, &status, 0);
          have_status = 1;
          *timed_out = 1;
          print_line_break ();
          print_formatted ("[task timed out after %d seconds]\n", command_timeout);
          break;
        }
    }
  while (!have_status && waitpid (child, &status, 0) < 0 && errno == EINTR)
    ;
  print_line_break ();
  close (pipe_fds[0]);
  if (WIFEXITED (status))
    *exit_status = WEXITSTATUS (status);
  else if (WIFSIGNALED (status))
    *exit_status = 128 + WTERMSIG (status);
  else
    *exit_status = 1;
  *output_length = captured;
  *dropped_length = dropped;
  output[captured] = 0;
  return 0;
}

/* Assemble the conversation record of a shell-mode command: what the human
   ran, the bounded output tail, and the exit status, tagged so the model can
   tell prompt-issued commands from its own tool calls. */
static int
build_shell_message (char *message, size_t capacity, const char *command, const char *output,
                     size_t output_length, size_t dropped, int timed_out, int status)
{
  Buffer body = { message, capacity, 0 };
  size_t tail = MODEL_OUTPUT_SIZE;
  size_t start = output_length > tail ? output_length - tail : 0;

  if (buffer_append_string (&body,
                            "HUMAN SHELL COMMAND (typed at the prompt with !, not a tool call):\n<bash-input>")
      < 0
      || append_xml_text (&body, command) < 0
      || buffer_append_string (&body, "</bash-input>\n<bash-output>\n") < 0)
    return -1;
  if (timed_out
      && buffer_append_formatted (&body, "[command timed out after %d seconds]\n", command_timeout) < 0)
    return -1;
  if (dropped > 0 && buffer_append_formatted (&body, "[... first %zu bytes dropped]\n", dropped) < 0)
    return -1;
  if (start != 0 && buffer_append_string (&body, "[output tail]\n") < 0)
    return -1;
  if (append_xml_escaped (&body, output + start, output_length - start) < 0
      || buffer_append_formatted (&body, "</bash-output>\n[exit status: %d]\n", status) < 0)
    return -1;
  message[body.length < capacity ? body.length : capacity - 1] = 0;
  return 0;
}

/* Shell mode: a submitted line starting with `!` runs the rest directly
   through the shell. The human typed it, so it needs no approval and no
   workspace scoping; the streamed output and exit status land in the
   conversation as context for the next request. */
static void
run_shell_prompt (char *command)
{
  static char output[CAPTURE_SIZE + 1];
  static char message[MAX_COMMAND * 6 + MODEL_OUTPUT_SIZE * 6 + 256];
  size_t output_length = 0;
  size_t dropped_length = 0;
  int status = 0;
  int timed_out = 0;

  while (*command == ' ' || *command == '\t')
    command++;
  if (*command == 0)
    {
      print_formatted ("usage: ! <command> runs it here; its output joins the conversation\n");
      return;
    }
  print_formatted ("\n[shell command]: %s\n", command);
  shell_direct = 1;
  if (run_command (command, output, CAPTURE_SIZE, &output_length, &dropped_length, &status, &timed_out) < 0)
    {
      shell_direct = 0;
      print_formatted ("error: cannot run command: %s\n", strerror (errno));
      return;
    }
  shell_direct = 0;
  if (timed_out)
    print_formatted ("[shell command timed out] exit status %d\n", status);
  else
    print_formatted ("[shell command completed] exit status %d\n", status);
  if (build_shell_message (message, sizeof (message), command, output, output_length, dropped_length,
                           timed_out, status)
      < 0)
    {
      print_formatted ("error: shell output cannot be represented within the bounded context\n");
      return;
    }
  if (add_text_message ("user", message, 0) < 0)
    print_formatted ("error: shell output cannot fit in the conversation\n");
}

static Job *
job_by_id (int id)
{
  size_t index;

  for (index = 0; index < MAX_JOBS; index++)
    if (jobs[index].used && jobs[index].id == id)
      return &jobs[index];
  return NULL;
}

static const char *
job_state_name (const Job *job)
{
  return job->state == JOB_RUNNING   ? "running"
         : job->state == JOB_STOPPED ? "stopped"
                                     : "exited";
}

/* The [jobs] marker rides regular output so the TUI status line and the
   plain terminal both see the live-job picture whenever it changes. */
static void
print_jobs_status (void)
{
  size_t index;
  int running = 0;
  long longest = 0;

  for (index = 0; index < MAX_JOBS; index++)
    if (jobs[index].used && jobs[index].state == JOB_RUNNING)
      {
        long elapsed = (long)(time (NULL) - jobs[index].started);

        running++;
        if (elapsed > longest)
          longest = elapsed;
      }
  if (running > 0)
    print_formatted ("[jobs] %d running (longest %lds)\n", running, longest);
  else
    print_formatted ("[jobs] none\n");
}

/* Harvest finished job children without blocking; statuses are fresh
   whenever the model can ask, which is whenever a round begins. */
static void
reap_jobs (void)
{
  size_t index;
  int changed = 0;

  for (index = 0; index < MAX_JOBS; index++)
    {
      int status;

      if (!jobs[index].used || jobs[index].state != JOB_RUNNING)
        continue;
      if (waitpid (jobs[index].pid, &status, WNOHANG) != jobs[index].pid)
        continue;
      if (WIFEXITED (status))
        jobs[index].exit_status = WEXITSTATUS (status);
      else if (WIFSIGNALED (status))
        jobs[index].exit_status = 128 + WTERMSIG (status);
      else
        jobs[index].exit_status = 1;
      jobs[index].finished = time (NULL);
      jobs[index].state = JOB_EXITED;
      print_formatted ("[job %d exited] status %d after %lds: %s\n", jobs[index].id, jobs[index].exit_status,
                       (long)(jobs[index].finished - jobs[index].started), jobs[index].command);
      changed = 1;
    }
  if (changed)
    print_jobs_status ();
}

/* Same escalation as command-timeout expiry: SIGTERM to the process
   group, a grace second, then SIGKILL. */
static void
job_terminate (Job *job)
{
  int status = 0;
  time_t deadline = time (NULL) + 1;

  kill (-job->pid, SIGTERM);
  while (time (NULL) < deadline)
    {
      if (waitpid (job->pid, &status, WNOHANG) == job->pid)
        goto finished;
      usleep (50000);
    }
  kill (-job->pid, SIGKILL);
  while (waitpid (job->pid, &status, 0) < 0 && errno == EINTR)
    ;
finished:
  if (WIFEXITED (status))
    job->exit_status = WEXITSTATUS (status);
  else if (WIFSIGNALED (status))
    job->exit_status = 128 + WTERMSIG (status);
  job->finished = time (NULL);
  job->state = JOB_STOPPED;
}

/* Jobs never outlive the agent silently: normal exit paths run this via
   atexit, terminate every live job, and say how many. */
static void
jobs_shutdown (void)
{
  size_t index;
  int stopped = 0;

  for (index = 0; index < MAX_JOBS; index++)
    if (jobs[index].used && jobs[index].state == JOB_RUNNING)
      {
        job_terminate (&jobs[index]);
        stopped++;
      }
  if (stopped > 0)
    print_formatted ("[%d job%s stopped]\n", stopped, stopped == 1 ? "" : "s");
}

/* Fork a run_command-style child with two changes: stdout and stderr
   append to <scope>/.ai/jobs/<id>.log and nobody waits. Returns 0 with
   *id_result set, -1 on system error, -2 when no slot is free. */
static int
job_spawn (const char *command, int *id_result)
{
  char log_directory[PATH_MAX + 16];
  char log_path[PATH_MAX + 32];
  const char *root = workspace_directory != NULL ? workspace_root : current_directory;
  int log_fd;
  pid_t child;
  size_t index;
  Job *slot = NULL;
  Job *oldest_finished = NULL;

  for (index = 0; index < MAX_JOBS; index++)
    {
      if (!jobs[index].used)
        {
          slot = &jobs[index];
          break;
        }
      if (jobs[index].state != JOB_RUNNING
          && (oldest_finished == NULL || jobs[index].finished < oldest_finished->finished))
        oldest_finished = &jobs[index];
    }
  if (slot == NULL)
    {
      if (oldest_finished == NULL)
        return -2;
      slot = oldest_finished;
    }
  snprintf (log_directory, sizeof (log_directory), "%s/.ai/jobs", root);
  mkdir (root, 0777);
  snprintf (log_path, sizeof (log_path), "%s/.ai", root);
  mkdir (log_path, 0777);
  mkdir (log_directory, 0777);
  slot->id = ++job_sequence;
  snprintf (log_path, sizeof (log_path), "%s/%d.log", log_directory, slot->id);
  log_fd = open (log_path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0644);
  if (log_fd < 0)
    return -1;
  child = fork ();
  if (child < 0)
    {
      close (log_fd);
      return -1;
    }
  if (child == 0)
    {
      struct sigaction action;

      memset (&action, 0, sizeof (action));
      action.sa_handler = SIG_DFL;
      sigaction (SIGINT, &action, NULL);
      sigaction (SIGTERM, &action, NULL);
      setpgid (0, 0);
      dup2 (log_fd, STDOUT_FILENO);
      dup2 (log_fd, STDERR_FILENO);
      if (log_fd != STDOUT_FILENO && log_fd != STDERR_FILENO)
        close (log_fd);
      /* Same rule as run_command children: scoped workspaces confine the
         executed tree; the human-approved start does not lift it. */
      if (workspace_directory != NULL && !full_access && !shell_direct && landlock_enabled)
        landlock_apply (workspace_root);
      execl ("/bin/sh", "sh", "-c", command, (char *)NULL);
      _exit (127);
    }
  setpgid (child, child);
  close (log_fd);
  memset (slot, 0, sizeof (*slot));
  slot->used = 1;
  slot->id = job_sequence;
  slot->pid = child;
  snprintf (slot->command, sizeof (slot->command), "%s", command);
  snprintf (slot->log_path, sizeof (slot->log_path), "%s", log_path);
  slot->started = time (NULL);
  slot->state = JOB_RUNNING;
  *id_result = slot->id;
  return 0;
}

/* Read the last `capacity` bytes of a job log for job_status; bytes
   before that are counted as dropped so the model knows the tail rule. */
static size_t
read_log_tail (const char *path, char *buffer, size_t capacity, size_t *dropped)
{
  FILE *log = fopen (path, "rb");
  long size;
  size_t got = 0;

  *dropped = 0;
  if (log == NULL)
    return 0;
  if (fseek (log, 0, SEEK_END) == 0)
    {
      size = ftell (log);
      if (size > (long)capacity)
        {
          *dropped = (size_t)size - capacity;
          fseek (log, - (long)capacity, SEEK_END);
        }
      else
        fseek (log, 0, SEEK_SET);
      got = fread (buffer, 1, capacity, log);
    }
  fclose (log);
  return got;
}

static int
read_interactive_line (const char *prompt, char *buffer, size_t capacity)
{
  size_t length;
  FILE *terminal = stdin;
  int from_tty = 0;

  if (one_shot && !isatty (STDIN_FILENO))
    {
      terminal = fopen ("/dev/tty", "r");
      if (terminal == NULL)
        {
          print_formatted ("error: command approval needs /dev/tty when stdin is piped\n");
          return -1;
        }
      from_tty = 1;
    }
  /* Terminal answers type on the prompt line like the multiline reader;
     piped consumers (the TUI) match whole prompt lines instead. */
  if (isatty (STDIN_FILENO) || (from_tty && isatty (STDOUT_FILENO)))
    print_formatted ("%s", prompt);
  else
    print_formatted ("%s\n", prompt);
  if (fgets (buffer, (int)capacity, terminal) == NULL)
    {
      if (from_tty)
        fclose (terminal);
      return -1;
    }
  if (from_tty)
    fclose (terminal);
  length = strlen (buffer);
  if (length > 0 && buffer[length - 1] == '\n')
    buffer[length - 1] = 0;
  else
    {
      int character;

      while ((character = getc (terminal)) != EOF && character != '\n')
        ;
    }
  return 0;
}

static int
read_interactive_multiline (const char *prompt, const char *continuation_prompt, char *buffer, size_t capacity)
{
  FILE *terminal;
  size_t length = 0;

  buffer[0] = 0;
  if (isatty (STDIN_FILENO) || !one_shot)
    return read_multiline (prompt, continuation_prompt, buffer, capacity);
  terminal = fopen ("/dev/tty", "r");
  if (terminal == NULL)
    {
      print_formatted ("error: interactive steering needs /dev/tty when stdin is piped\n");
      return -1;
    }
  if (isatty (STDOUT_FILENO))
    print_formatted ("%s", prompt);
  else
    print_formatted ("%s\n", prompt);
  for (;;)
    {
      char line[MAX_LINE];

      if (fgets (line, sizeof (line), terminal) == NULL)
        break;
      line[strcspn (line, "\n")] = 0;
      if (line[0] == 0)
        {
          fclose (terminal);
          return 0;
        }
      if (length + strlen (line) + 2 >= capacity)
        {
          print_formatted ("error: input is too long\n");
          fclose (terminal);
          return -1;
        }
      if (length != 0)
        buffer[length++] = '\n';
      memcpy (buffer + length, line, strlen (line) + 1);
      length += strlen (line);
      print_formatted ("%s\n", continuation_prompt);
    }
  fclose (terminal);
  return length == 0 ? -1 : 0;
}

static int
command_decision (ToolCall *call, int *run, char *steer, size_t steer_capacity)
{
  char choice[MAX_LINE];

  *run = 0;
  steer[0] = 0;
  for (;;)
    {
      if (read_interactive_line ("[a]llow, [e]dit, [s]teer without running, [n]o, [f]orce, [q]uit: ", choice,
                                 sizeof (choice))
          < 0)
        return -1;
      if (choice[0] == 'a' && choice[1] == 0)
        {
          *run = 1;
          return 0;
        }
      if (choice[0] == 'e' && choice[1] == 0)
        {
          if (read_interactive_line ("[edited command]: ", call->command, sizeof (call->command)) < 0
              || call->command[0] == 0)
            continue;
          print_formatted ("[AI command edited to]: %s\n", call->command);
          *run = 1;
          return 0;
        }
      if (choice[0] == 'f' && choice[1] == 0)
        {
          full_access = 1;
          print_formatted ("[force mode is now ON; commands run without approval]\n");
          *run = 1;
          return 0;
        }
      if (choice[0] == 's' && choice[1] == 0)
        {
          print_formatted (multiline_keys_active () ? "Steering: C-J newline, Enter submit.\n"
                                                    : "Steering ends with an empty line.\n");
          if (read_interactive_multiline ("[steer]: ", "[.....]: ", steer, steer_capacity) < 0 || steer[0] == 0)
            continue;
          return 0;
        }
      if ((choice[0] == 'n' || choice[0] == 'q') && choice[1] == 0)
        return choice[0] == 'q' ? -1 : 0;
      print_formatted ("Please answer a, e, s, n, f, or q.\n");
    }
}

static int
patch_decision (int *run, char *steer, size_t steer_capacity)
{
  char choice[MAX_LINE];

  *run = 0;
  steer[0] = 0;
  for (;;)
    {
      if (read_interactive_line ("[a]llow, [s]teer without applying, [n]o, [f]orce, [q]uit: ", choice,
                                 sizeof (choice))
          < 0)
        return -1;
      if (choice[0] == 'a' && choice[1] == 0)
        {
          *run = 1;
          return 0;
        }
      if (choice[0] == 'f' && choice[1] == 0)
        {
          full_access = 1;
          print_formatted ("[force mode is now ON; patches apply without approval]\n");
          *run = 1;
          return 0;
        }
      if (choice[0] == 's' && choice[1] == 0)
        {
          print_formatted (multiline_keys_active () ? "Steering: C-J newline, Enter submit.\n"
                                                    : "Steering ends with an empty line.\n");
          if (read_interactive_multiline ("[steer]: ", "[.....]: ", steer, steer_capacity) < 0 || steer[0] == 0)
            continue;
          return 0;
        }
      if ((choice[0] == 'n' || choice[0] == 'q') && choice[1] == 0)
        return choice[0] == 'q' ? -1 : 0;
      print_formatted ("Please answer a, s, n, f, or q.\n");
    }
}

/* Stopping a job the human wanted running is also a decision, so it gets
   the same boundary as a patch: no edit option, the command is fixed. */
static int
stop_decision (int *run, char *steer, size_t steer_capacity)
{
  char choice[MAX_LINE];

  *run = 0;
  steer[0] = 0;
  for (;;)
    {
      if (read_interactive_line ("[a]llow, [s]teer without stopping, [n]o, [f]orce, [q]uit: ", choice,
                                 sizeof (choice))
          < 0)
        return -1;
      if (choice[0] == 'a' && choice[1] == 0)
        {
          *run = 1;
          return 0;
        }
      if (choice[0] == 'f' && choice[1] == 0)
        {
          full_access = 1;
          print_formatted ("[force mode is now ON; jobs stop without approval]\n");
          *run = 1;
          return 0;
        }
      if (choice[0] == 's' && choice[1] == 0)
        {
          print_formatted (multiline_keys_active () ? "Steering: C-J newline, Enter submit.\n"
                                                    : "Steering ends with an empty line.\n");
          if (read_interactive_multiline ("[steer]: ", "[.....]: ", steer, steer_capacity) < 0 || steer[0] == 0)
            continue;
          return 0;
        }
      if ((choice[0] == 'n' || choice[0] == 'q') && choice[1] == 0)
        return choice[0] == 'q' ? -1 : 0;
      print_formatted ("Please answer a, s, n, f, or q.\n");
    }
}

static int
post_execution_steer (char *steer, size_t capacity)
{
  char answer[MAX_LINE];

  for (;;)
    {
      if (!steering_available () || full_access)
        return 0;
      if (read_interactive_line ("[s]teer before next AI request, Enter to continue, q quit: ", answer, sizeof (answer))
          < 0)
        return -1;
      if (answer[0] == 0)
        return 0;
      if (answer[0] == 'q' && answer[1] == 0)
        return -1;
      if (answer[0] == 's' && answer[1] == 0)
        {
          print_formatted (multiline_keys_active () ? "Steering: C-J newline, Enter submit.\n"
                                                    : "Steering ends with an empty line.\n");
          if (read_interactive_multiline ("[steer]: ", "[.....]: ", steer, capacity) < 0 || steer[0] == 0)
            continue;
          return 0;
        }
      print_formatted ("Please answer s, Enter, or q.\n");
    }
}

static int
normalize_scoped_path (const char *path, char *resolved, size_t capacity)
{
  char normalized[PATH_MAX];

  if (path[0] == '/')
    {
      if (realpath (path, resolved) != NULL)
        return 0;
      if (strlen (path) >= capacity)
        return -1;
      strcpy (resolved, path);
      return 0;
    }
  if (snprintf (normalized, sizeof (normalized), "%s/%s", workspace_root, path) >= (int)sizeof (normalized))
    return -1;
  if (realpath (normalized, resolved) != NULL)
    return 0;
  if (strlen (normalized) >= capacity)
    return -1;
  strcpy (resolved, normalized);
  return 0;
}

static int
contains_outside_path (const char *command)
{
  const char *start = command;
  char candidate[PATH_MAX];
  char resolved[PATH_MAX];

  while (*start != 0)
    {
      const char *end = start;
      int absolute = *start == '/';
      char first;
      const char *slash;

      while (*end != 0 && !isspace ((unsigned char)*end))
        end++;
      if (end > start)
        {
          size_t length = (size_t)(end - start);

          if (absolute)
            {
              if (length >= sizeof (candidate))
                return 1;
              memcpy (candidate, start, length);
              candidate[length] = 0;
              if (normalize_scoped_path (candidate, resolved, sizeof (resolved)) == 0
                  && (strncmp (resolved, workspace_root, strlen (workspace_root)) != 0
                      || (resolved[strlen (workspace_root)] != 0 && resolved[strlen (workspace_root)] != '/')))
                return 1;
            }
          else if (length >= 2 && strncmp (start, "..", 2) == 0)
            {
              if (snprintf (candidate, sizeof (candidate), "%s/%.*s", workspace_root, (int)length, start)
                  >= (int)sizeof (candidate))
                return 1;
              if (realpath (candidate, resolved) != NULL)
                {
                  if (strncmp (resolved, workspace_root, strlen (workspace_root)) != 0
                      || (resolved[strlen (workspace_root)] != 0 && resolved[strlen (workspace_root)] != '/'))
                    return 1;
                }
              else
                {
                  slash = start + 2;
                  first = slash != end ? *slash : '/';
                  if (first == '/' || first == 0)
                    return 1;
                }
            }
        }
      start = *end == 0 ? end : end + 1;
    }
  return 0;
}

static int
validate_workspace_command (const char *command)
{
  if (workspace_directory == NULL || !contains_outside_path (command))
    return 0;
  print_formatted ("Command blocked: it references a path outside the workspace scope %s.\n", workspace_root);
  return -1;
}

static int
append_bounded_output (const ToolCall *call, const char *prefix, const char *output, size_t output_length,
                       size_t dropped)
{
  char prefix_text[MAX_LINE];
  char drop_text[64];
  size_t tail = MODEL_OUTPUT_SIZE;
  size_t start;
  size_t prefix_length;

  if (dropped > 0)
    snprintf (drop_text, sizeof (drop_text), "[... first %zu bytes dropped]\n", dropped);
  else
    drop_text[0] = 0;
  for (;;)
    {
      start = output_length > tail ? output_length - tail : 0;
      prefix_length = (size_t)snprintf (prefix_text, sizeof (prefix_text), "%s\n%s%s", prefix, drop_text,
                                        start != 0 ? "[output tail]\n" : "");
      if (prefix_length >= sizeof (prefix_text))
        return -1;
      if (append_tool_output (call, prefix_text, output + start, output_length - start) == 0)
        return 0;
      if (tail <= 256)
        return append_tool_result (call, "Command output could not be represented within the bounded context.");
      tail /= 2;
    }
}

/* The plan tool only moves agent-held state, so it needs no command
   approval; printing the new plan keeps the human watching. */
static void
record_plan (const char *checklist)
{
  char trimmed[MAX_PLAN];

  snprintf (trimmed, sizeof (trimmed), "%s", checklist);
  trim_spaces (trimmed);
  if (trimmed[0] == 0)
    {
      plan_active = 0;
      current_plan[0] = 0;
      session_log_plan ("");
      print_formatted ("[Plan cleared]\n");
      return;
    }
  snprintf (current_plan, sizeof (current_plan), "%s", trimmed);
  plan_active = 1;
  session_log_plan (current_plan);
  print_formatted ("[Plan updated]:\n");
  print_formatted ("%s\n", current_plan);
}

/* Echo the raw patch text so the human can review the exact hunks before
   deciding; print_formatted handles the pager-style flow control. */
static void
print_patch_text (const char *patch)
{
  const char *line = patch;

  while (*line != 0)
    {
      const char *end = strchr (line, '\n');
      size_t length = end != NULL ? (size_t) (end - line) : strlen (line);

      print_formatted ("%.*s\n", (int) length, line);
      if (end == NULL)
        break;
      line = end + 1;
    }
}

static int
process_tool_calls (ToolCall *calls, size_t call_count)
{
  static char output[CAPTURE_SIZE + 1];
  char steer[MAX_COMMAND];
  const char *scope_root = workspace_directory != NULL ? workspace_root : current_directory;
  size_t index;

  steer[0] = 0;
  reap_jobs ();
  for (index = 0; index < call_count; index++)
    {
      int execute = 0;
      int status;
      int timed_out = 0;
      size_t output_length = 0;
      size_t dropped_length = 0;
      char result[MAX_LINE];
      ToolResult tool;

      if (calls[index].valid && calls[index].kind == TOOL_PLAN)
        {
          record_plan (calls[index].checklist);
          if (append_tool_result (&calls[index],
                                  plan_active ? "Plan recorded; it stays pinned in every later request."
                                              : "Plan cleared.")
              < 0)
            return -1;
          continue;
        }
      if (calls[index].valid && calls[index].kind == TOOL_READ_FILE)
        {
          print_formatted ("\n[AI reads file]: %s\n", calls[index].path);
          if (tool_read_file (scope_root, calls[index].path, calls[index].offset, calls[index].limit, &tool) < 0
              || tool.failed)
            {
              print_formatted ("%s\n", tool.status);
              if (append_tool_result (&calls[index], tool.status) < 0)
                return -1;
              continue;
            }
          /* The human watches the same numbered lines the model receives,
             written raw like live command output so nothing is concealed. */
          write_all (STDOUT_FILENO, tool.body, tool.body_length);
          if (append_bounded_output (&calls[index], tool.status, tool.body, tool.body_length, 0) < 0)
            return -1;
          continue;
        }
      if (calls[index].valid && calls[index].kind == TOOL_LIST_DIR)
        {
          print_formatted ("\n[AI lists dir]: %s\n", calls[index].path);
          if (tool_list_dir (scope_root, calls[index].path, &tool) < 0 || tool.failed)
            {
              print_formatted ("%s\n", tool.status);
              if (append_tool_result (&calls[index], tool.status) < 0)
                return -1;
              continue;
            }
          write_all (STDOUT_FILENO, tool.body, tool.body_length);
          if (append_bounded_output (&calls[index], tool.status, tool.body, tool.body_length, 0) < 0)
            return -1;
          continue;
        }
      if (calls[index].valid && calls[index].kind == TOOL_APPLY_PATCH)
        {
          char offender[PATH_MAX];

          print_formatted ("\n[AI requests patch]\n");
          print_patch_text (calls[index].patch);
          if (tool_patch_confined (scope_root, calls[index].patch, offender, sizeof (offender)) < 0)
            {
              print_formatted ("Patch blocked: %s resolves outside the workspace scope %s.\n", offender, scope_root);
              if (append_tool_result (&calls[index],
                                      "Error: the patch references a path outside the workspace scope.")
                  < 0)
                return -1;
              continue;
            }
          if (steer[0] != 0)
            execute = 0;
          else if (full_access)
            execute = 1;
          else if (patch_decision (&execute, steer, sizeof (steer)) < 0)
            return -1;
          if (execute)
            {
              if (tool_apply_patch (scope_root, calls[index].patch, &tool) < 0 || tool.failed)
                {
                  print_formatted ("%s\n", tool.status);
                  if (append_tool_result (&calls[index], tool.status) < 0)
                    return -1;
                  if (post_execution_steer (steer, sizeof (steer)) < 0)
                    return -1;
                  continue;
                }
              print_formatted ("%s\n", tool.status);
              if (append_tool_result (&calls[index], tool.status) < 0)
                return -1;
              if (post_execution_steer (steer, sizeof (steer)) < 0)
                return -1;
            }
          else
            {
              snprintf (result, sizeof (result), "%s",
                        steer[0] != 0 ? "Patch skipped because the human supplied a new direction."
                                      : "Patch refused by the human.");
              if (append_tool_result (&calls[index], result) < 0)
                return -1;
            }
          continue;
        }
      if (calls[index].valid && calls[index].kind == TOOL_START_JOB)
        {
          int job_id = 0;

          print_formatted ("\n[AI starts job]: %s\n", calls[index].command);
          if (no_shell)
            {
              if (append_tool_result (&calls[index], "Error: background jobs are disabled by --no-shell.") < 0)
                return -1;
              continue;
            }
          if (validate_workspace_command (calls[index].command) < 0)
            {
              if (append_tool_result (&calls[index], "Error: command references a path outside the workspace scope.")
                  < 0)
                return -1;
              continue;
            }
          if (steer[0] != 0)
            execute = 0;
          else if (full_access)
            execute = 1;
          else if (command_decision (&calls[index], &execute, steer, sizeof (steer)) < 0)
            return -1;
          if (execute)
            {
              int spawned = job_spawn (calls[index].command, &job_id);

              if (spawned < 0)
                {
                  snprintf (result, sizeof (result), "%s", spawned == -2
                              ? "Error: too many jobs are running; stop one with stop_job before starting another."
                              : "Execution error: cannot start the job.");
                  if (append_tool_result (&calls[index], result) < 0)
                    return -1;
                  continue;
                }
              print_formatted ("[job %d started] log: %s\n", job_id, job_by_id (job_id)->log_path);
              print_jobs_status ();
              snprintf (result, sizeof (result),
                        "[job %d started] Output appends to the job log; it is exempt from the command timeout. "
                        "Poll with job_status %d and stop it with stop_job %d when done.",
                        job_id, job_id, job_id);
              if (append_tool_result (&calls[index], result) < 0)
                return -1;
              if (post_execution_steer (steer, sizeof (steer)) < 0)
                return -1;
            }
          else
            {
              snprintf (result, sizeof (result), "%s",
                        steer[0] != 0 ? "Job skipped because the human supplied a new direction."
                                      : "Job refused by the human.");
              if (append_tool_result (&calls[index], result) < 0)
                return -1;
            }
          continue;
        }
      if (calls[index].valid && calls[index].kind == TOOL_JOB_STATUS)
        {
          size_t job_index;

          reap_jobs ();
          if (calls[index].job_id == 0)
            {
              char listing[MAX_JOBS * 192 + 64];
              Buffer summary = { listing, sizeof (listing), 0 };
              int printed = 0;

              for (job_index = 0; job_index < MAX_JOBS; job_index++)
                if (jobs[job_index].used)
                  {
                    long elapsed = (long)(time (NULL) - jobs[job_index].started);
                    const char *state = job_state_name (&jobs[job_index]);

                    printed++;
                    print_formatted ("[job %d] %s, %lds: %s\n", jobs[job_index].id, state, elapsed,
                                     jobs[job_index].command);
                    if (jobs[job_index].state == JOB_RUNNING)
                      {
                        if (buffer_append_formatted (&summary, "%sjob %d: running for %lds: %s",
                                                     printed == 1 ? "" : "\n", jobs[job_index].id, elapsed,
                                                     jobs[job_index].command)
                            < 0)
                          break;
                      }
                    else if (buffer_append_formatted (&summary, "%sjob %d: %s after %lds, exit status %d: %s",
                                                      printed == 1 ? "" : "\n", jobs[job_index].id, state, elapsed,
                                                      jobs[job_index].exit_status, jobs[job_index].command)
                             < 0)
                      break;
                  }
              if (printed == 0)
                {
                  print_formatted ("[job status] no jobs\n");
                  if (append_tool_result (&calls[index], "No jobs have been started.") < 0)
                    return -1;
                  continue;
                }
              listing[summary.length < sizeof (listing) ? summary.length : sizeof (listing) - 1] = 0;
              if (append_tool_result (&calls[index], listing) < 0)
                return -1;
              continue;
            }
          {
            Job *job = job_by_id (calls[index].job_id);

            if (job == NULL)
              {
                snprintf (result, sizeof (result), "Error: no job with id %d; job_status without id lists them.",
                          calls[index].job_id);
                print_formatted ("%s\n", result);
                if (append_tool_result (&calls[index], result) < 0)
                  return -1;
                continue;
              }
            {
              long elapsed = (long)(time (NULL) - job->started);
              const char *state = job_state_name (job);

              print_formatted ("[job %d] %s: %s\n", job->id, state, job->command);
              if (job->state == JOB_RUNNING)
                snprintf (result, sizeof (result), "job %d: running, %lds elapsed. Log tail:", job->id, elapsed);
              else
                snprintf (result, sizeof (result), "job %d: %s after %lds, exit status %d. Log tail:", job->id, state,
                          (long)(job->finished - job->started), job->exit_status);
              output_length = read_log_tail (job->log_path, output, CAPTURE_SIZE, &dropped_length);
              /* The human watches the same log tail the model receives,
                 written raw like live command output so nothing is concealed.
                 The raw write skips print_formatted's bookkeeping, so track
                 the partial line here or later notices would glue to it. */
              write_all (STDOUT_FILENO, output, output_length);
              output_partial_line = output_length > 0 && output[output_length - 1] != '\n';
              print_line_break ();
              if (append_bounded_output (&calls[index], result, output, output_length, dropped_length) < 0)
                return -1;
            }
          }
          continue;
        }
      if (calls[index].valid && calls[index].kind == TOOL_STOP_JOB)
        {
          Job *job;

          print_formatted ("\n[AI stops job]: %d\n", calls[index].job_id);
          if (no_shell)
            {
              if (append_tool_result (&calls[index], "Error: background jobs are disabled by --no-shell.") < 0)
                return -1;
              continue;
            }
          if (steer[0] != 0)
            execute = 0;
          else if (full_access)
            execute = 1;
          else if (stop_decision (&execute, steer, sizeof (steer)) < 0)
            return -1;
          if (!execute)
            {
              snprintf (result, sizeof (result), "%s",
                        steer[0] != 0 ? "Stop skipped because the human supplied a new direction."
                                      : "Stop refused by the human.");
              if (append_tool_result (&calls[index], result) < 0)
                return -1;
              continue;
            }
          reap_jobs ();
          job = job_by_id (calls[index].job_id);
          if (job == NULL)
            {
              snprintf (result, sizeof (result), "Error: no job with id %d; job_status without id lists them.",
                        calls[index].job_id);
              print_formatted ("%s\n", result);
              if (append_tool_result (&calls[index], result) < 0)
                return -1;
              continue;
            }
          if (job->state == JOB_RUNNING)
            {
              job_terminate (job);
              print_formatted ("[job %d stopped] status %d\n", job->id, job->exit_status);
              print_jobs_status ();
              snprintf (result, sizeof (result), "[job %d stopped] exit status %d. Log tail:", job->id,
                        job->exit_status);
            }
          else
            snprintf (result, sizeof (result), "[job %d already %s] exit status %d. Log tail:", job->id,
                      job_state_name (job), job->exit_status);
          output_length = read_log_tail (job->log_path, output, CAPTURE_SIZE, &dropped_length);
          write_all (STDOUT_FILENO, output, output_length);
          print_line_break ();
          if (append_bounded_output (&calls[index], result, output, output_length, dropped_length) < 0)
            return -1;
          if (post_execution_steer (steer, sizeof (steer)) < 0)
            return -1;
          continue;
        }
      if (calls[index].valid && calls[index].kind == TOOL_TASK)
        {
          print_formatted ("\n[AI delegates task]:\n");
          print_formatted ("%s\n", calls[index].command);
          if (subagent_depth)
            {
              if (append_tool_result (&calls[index],
                                      "Error: delegation is disabled inside subagents; handle the subtask yourself.")
                  < 0)
                return -1;
              continue;
            }
          /* Terminal mode needs no gate here: the child's own command
             approvals reach the human through /dev/tty. Under the TUI they
             cannot, so an `a` runs the child --no-shell and only `f` (or
             prior full access) grants it commands without approval. */
          if (steer[0] != 0)
            execute = 0;
          else if (full_access)
            execute = 1;
          else if (command_decision (&calls[index], &execute, steer, sizeof (steer)) < 0)
            return -1;
          if (execute)
            {
              if (task_run (calls[index].command, output, CAPTURE_SIZE, &output_length, &dropped_length, &status,
                            &timed_out)
                  < 0)
                {
                  if (append_tool_result (&calls[index],
                                          "Error: cannot start the subagent; fork or /proc/self/exe failed.")
                      < 0)
                    return -1;
                  continue;
                }
              if (timed_out)
                snprintf (result, sizeof (result), "Subagent timed out after %d seconds. Output tail:",
                          command_timeout);
              else
                snprintf (result, sizeof (result), "Subagent finished, exit status %d. Output tail:", status);
              if (append_bounded_output (&calls[index], result, output, output_length, dropped_length) < 0)
                return -1;
              if (post_execution_steer (steer, sizeof (steer)) < 0)
                return -1;
            }
          else
            {
              snprintf (result, sizeof (result), "%s",
                        steer[0] != 0 ? "Task skipped because the human supplied a new direction."
                                      : "Task refused by the human.");
              if (append_tool_result (&calls[index], result) < 0)
                return -1;
            }
          continue;
        }
      if (calls[index].valid && calls[index].supported)
        {
          print_formatted ("\n[AI requests command]: %s\n", calls[index].command);
          if (no_shell)
            {
              if (append_tool_result (&calls[index],
                                      "Error: the terminal tool is disabled by --no-shell; use read_file, list_dir, "
                                      "or apply_patch.")
                  < 0)
                return -1;
              continue;
            }
          if (validate_workspace_command (calls[index].command) < 0)
            {
              if (append_tool_result (&calls[index], "Error: command references a path outside the workspace scope.")
                  < 0)
                return -1;
              continue;
            }
          if (steer[0] != 0)
            execute = 0;
          else if (full_access)
            execute = 1;
          else if (command_decision (&calls[index], &execute, steer, sizeof (steer)) < 0)
            return -1;
        }
      if (execute && calls[index].valid && calls[index].supported)
        {
          if (run_command (calls[index].command, output, CAPTURE_SIZE, &output_length, &dropped_length, &status,
                           &timed_out)
              < 0)
            {
              snprintf (result, sizeof (result), "Execution error: %s", strerror (errno));
              if (append_tool_result (&calls[index], result) < 0)
                return -1;
              continue;
            }
          if (timed_out)
            snprintf (result, sizeof (result), "[command timed out after %d seconds] exit status %d. Output:",
                      command_timeout, status);
          else
            snprintf (result, sizeof (result), "[command completed] exit status %d. Output:", status);
          if (append_bounded_output (&calls[index], result, output, output_length, dropped_length) < 0)
            return -1;
          if (post_execution_steer (steer, sizeof (steer)) < 0)
            return -1;
        }
      else if (!calls[index].valid)
        {
          print_formatted ("Error: malformed or missing tool arguments.\n");
          if (append_tool_result (&calls[index], "Error: malformed or missing tool arguments.") < 0)
            return -1;
        }
      else if (!calls[index].supported)
        {
          if (append_tool_result (&calls[index],
                                  "Error: unsupported tool; use run_terminal_command, read_file, list_dir, "
                                  "apply_patch, plan, task, start_job, job_status, or stop_job.")
              < 0)
            return -1;
        }
      else
        {
          snprintf (result, sizeof (result), "%s",
                    steer[0] != 0 ? "Execution skipped because the human supplied a new direction."
                                  : "Execution refused by the human.");
          if (append_tool_result (&calls[index], result) < 0)
            return -1;
        }
    }
  if (append_live_steering (steer, sizeof (steer)) < 0)
    return -1;
  if (steer[0] != 0)
    {
      char user_message[MAX_COMMAND + 64];

      snprintf (user_message, sizeof (user_message), "HUMAN STEERING DURING TOOL EXECUTION: %s", steer);
      if (add_text_message ("user", user_message, 0) < 0)
        return -1;
    }
  return 0;
}

static int
add_pending_request_steering (void)
{
  char steering[MAX_COMMAND];

  if (!steering_available ())
    return 0;
  steering[0] = 0;
  if (append_live_steering (steering, sizeof (steering)) < 0)
    return -1;
  if (steering[0] == 0)
    return 0;
  {
    char user_message[MAX_COMMAND + 64];

    snprintf (user_message, sizeof (user_message), "HUMAN STEERING DURING AI REQUEST: %s", steering);
    return add_text_message ("user", user_message, 0);
  }
}

static int
parse_tool_call (const cJSON *tool_call, ToolCall *call)
{
  const cJSON *function;
  const cJSON *id;
  const cJSON *name;
  const cJSON *arguments;
  cJSON *argument_object;
  const cJSON *command;

  memset (call, 0, sizeof (*call));
  strcpy (call->id, "tool-0");
  strcpy (call->name, "unknown");
  call->valid = 0;
  if (!cJSON_IsObject (tool_call))
    return -1;

  id = cJSON_GetObjectItemCaseSensitive (tool_call, "id");
  if (id != NULL && copy_json_string (id, call->id, sizeof (call->id)) < 0)
    return -1;
  function = cJSON_GetObjectItemCaseSensitive (tool_call, "function");
  if (!cJSON_IsObject (function))
    return -1;

  name = cJSON_GetObjectItemCaseSensitive (function, "name");
  if (copy_json_string (name, call->name, sizeof (call->name)) < 0)
    return -1;
  if (strcmp (call->name, "plan") == 0)
    call->kind = TOOL_PLAN;
  else if (strcmp (call->name, "run_terminal_command") == 0)
    call->kind = TOOL_TERMINAL;
  else if (strcmp (call->name, "read_file") == 0)
    call->kind = TOOL_READ_FILE;
  else if (strcmp (call->name, "list_dir") == 0)
    call->kind = TOOL_LIST_DIR;
  else if (strcmp (call->name, "apply_patch") == 0)
    call->kind = TOOL_APPLY_PATCH;
  else if (strcmp (call->name, "start_job") == 0)
    call->kind = TOOL_START_JOB;
  else if (strcmp (call->name, "job_status") == 0)
    call->kind = TOOL_JOB_STATUS;
  else if (strcmp (call->name, "stop_job") == 0)
    call->kind = TOOL_STOP_JOB;
  else if (strcmp (call->name, "task") == 0)
    call->kind = TOOL_TASK;
  else
    call->kind = TOOL_UNKNOWN;
  call->supported = call->kind != TOOL_UNKNOWN;

  arguments = cJSON_GetObjectItemCaseSensitive (function, "arguments");
  if (!cJSON_IsString (arguments))
    return -1;
  argument_object = cJSON_Parse (cJSON_GetStringValue (arguments));
  if (argument_object == NULL)
    return -1;

  if (call->kind == TOOL_PLAN)
    {
      const cJSON *checklist = cJSON_GetObjectItemCaseSensitive (argument_object, "checklist");

      /* A missing or oversized checklist stays visible as a rejected plan
         call instead of being replaced by a shell tool call. */
      call->valid = copy_json_string (checklist, call->checklist, sizeof (call->checklist)) == 0;
      cJSON_Delete (argument_object);
      return 0;
    }

  if (call->kind == TOOL_READ_FILE || call->kind == TOOL_LIST_DIR)
    {
      const cJSON *path = cJSON_GetObjectItemCaseSensitive (argument_object, "path");

      if (copy_json_string (path, call->path, sizeof (call->path)) < 0)
        {
          cJSON_Delete (argument_object);
          return -1;
        }
      call->offset = 1;
      call->limit = 0;
      if (call->kind == TOOL_READ_FILE)
        {
          const cJSON *offset = cJSON_GetObjectItemCaseSensitive (argument_object, "offset");
          const cJSON *limit = cJSON_GetObjectItemCaseSensitive (argument_object, "limit");

          if (cJSON_IsNumber (offset))
            call->offset = (long) offset->valuedouble;
          if (call->offset < 1)
            call->offset = 1;
          if (cJSON_IsNumber (limit))
            call->limit = (long) limit->valuedouble;
          if (call->limit < 0)
            call->limit = 0;
        }
      cJSON_Delete (argument_object);
      call->valid = call->path[0] != 0;
      return 0;
    }

  if (call->kind == TOOL_APPLY_PATCH)
    {
      const cJSON *patch = cJSON_GetObjectItemCaseSensitive (argument_object, "patch");

      if (copy_json_string (patch, call->patch, sizeof (call->patch)) < 0)
        {
          cJSON_Delete (argument_object);
          return -1;
        }
      cJSON_Delete (argument_object);
      call->valid = call->patch[0] != 0;
      return 0;
    }

  if (call->kind == TOOL_JOB_STATUS || call->kind == TOOL_STOP_JOB)
    {
      const cJSON *id = cJSON_GetObjectItemCaseSensitive (argument_object, "id");

      call->job_id = 0;
      if (cJSON_IsNumber (id))
        call->job_id = (int)id->valuedouble;
      cJSON_Delete (argument_object);
      /* job_status without an id lists every job; stop_job needs one. */
      call->valid = call->kind == TOOL_JOB_STATUS || call->job_id >= 1;
      return 0;
    }

  if (call->kind == TOOL_TASK)
    {
      const cJSON *prompt = cJSON_GetObjectItemCaseSensitive (argument_object, "prompt");

      /* The prompt rides in call->command, bounded like an -exec prompt. */
      if (copy_json_string (prompt, call->command, sizeof (call->command)) < 0)
        {
          cJSON_Delete (argument_object);
          return -1;
        }
      cJSON_Delete (argument_object);
      call->valid = call->command[0] != 0;
      return 0;
    }

  command = cJSON_GetObjectItemCaseSensitive (argument_object, "command");
  if (copy_json_string (command, call->command, sizeof (call->command)) < 0)
    {
      cJSON_Delete (argument_object);
      return -1;
    }
  cJSON_Delete (argument_object);
  call->valid = call->command[0] != 0;
  return 0;
}

static void
record_usage (const cJSON *response)
{
  const cJSON *stats = cJSON_GetObjectItemCaseSensitive (response, "usage");
  const cJSON *item;

  response_rounds++;
  if (!cJSON_IsObject (stats))
    return;
  item = cJSON_GetObjectItemCaseSensitive (stats, "prompt_tokens");
  if (cJSON_IsNumber (item))
    {
      last_prompt_tokens = (long)item->valuedouble;
      total_prompt_tokens += last_prompt_tokens;
    }
  item = cJSON_GetObjectItemCaseSensitive (stats, "completion_tokens");
  if (cJSON_IsNumber (item))
    total_completion_tokens += (long)item->valuedouble;
}

static void
format_token_count (char *buffer, size_t capacity, long tokens)
{
  if (tokens < 1000)
    snprintf (buffer, capacity, "%ld", tokens);
  else
    snprintf (buffer, capacity, "%.1fk", (double)tokens / 1000.0);
}

static void
print_usage_summary (void)
{
  char in_text[24];
  char out_text[24];
  char context_text[24];
  char compact_text[32];

  if (response_rounds == 0)
    return;
  format_token_count (in_text, sizeof (in_text), total_prompt_tokens);
  format_token_count (out_text, sizeof (out_text), total_completion_tokens);
  format_token_count (context_text, sizeof (context_text), last_prompt_tokens);
  if (compaction_count > 0)
    snprintf (compact_text, sizeof (compact_text), ", compacted ×%d", compaction_count);
  else
    compact_text[0] = 0;
  /* Keep the summary off a partial line, such as the prompt label left
     behind by Ctrl+C at an idle prompt. */
  print_line_break ();
  print_formatted ("[usage] tokens: %s in / %s out, %ld rounds, context %s%s\n", in_text, out_text, response_rounds,
                   context_text, compact_text);
}

/* Context compaction: fold messages[start..end) into the pinned summary
   with one buffered summarize request, so pruning keeps the task thread
   instead of deleting it. Any failure returns -1 and the caller falls back
   to plain pruning; success returns the number of messages removed. */
static int
compact_messages (size_t start, size_t end)
{
  static char body[MAX_SUMMARY_REQUEST];
  static char reply[MAX_RESPONSE];
  static const char fragment_prefix[] = "\n\nConversation fragments to fold (JSON messages):\n";
  const char *response = NULL;
  cJSON *parsed;
  const cJSON *choices;
  const cJSON *message;
  const cJSON *content;
  Buffer buffer = { body, sizeof (body), 0 };
  size_t index;
  size_t summary_length;
  int transient = 0;
  int retry_after = 0;

  if (end <= start || end > message_count)
    return -1;
  if (buffer_append_string (&buffer, "{\"model\":") < 0 || append_json_text (&buffer, model) < 0
      || buffer_append_string (&buffer, ",\"messages\":[{\"role\":\"system\",\"content\":") < 0
      || append_json_text (&buffer, summarize_prompt) < 0
      || buffer_append_string (&buffer, "},{\"role\":\"user\",\"content\":\"") < 0
      || append_json_string_body (&buffer, "Existing summary:\n", strlen ("Existing summary:\n")) < 0)
    return -1;
  if (summary_active)
    {
      if (append_json_string_body (&buffer, context_summary, strlen (context_summary)) < 0)
        return -1;
    }
  else if (append_json_string_body (&buffer, "(none)", strlen ("(none)")) < 0)
    return -1;
  if (append_json_string_body (&buffer, fragment_prefix, sizeof (fragment_prefix) - 1) < 0)
    return -1;
  for (index = start; index < end; index++)
    {
      if (append_json_string_body (&buffer, messages[index].json, messages[index].length) < 0
          || append_json_string_body (&buffer, "\n", 1) < 0)
        return -1;
    }
  if (buffer_append_string (&buffer, "\"}]}") < 0)
    return -1;
  /* Buffered, single-shot, nothing streamed: this is internal bookkeeping,
     and a failure simply falls back to plain pruning. Single attempt, so
     its reason line is a final one. */
  if (post_endpoint_once (body, buffer.length, &response, reply, &transient, &retry_after, 1) < 0)
    return -1;
  parsed = cJSON_Parse (response);
  if (parsed == NULL)
    return -1;
  record_usage (parsed);
  choices = cJSON_GetObjectItemCaseSensitive (parsed, "choices");
  message = cJSON_GetObjectItemCaseSensitive (cJSON_GetArrayItem (choices, 0), "message");
  content = cJSON_GetObjectItemCaseSensitive (message, "content");
  if (!cJSON_IsString (content) || content->valuestring[0] == 0)
    {
      cJSON_Delete (parsed);
      return -1;
    }
  summary_length = strlen (content->valuestring);
  if (summary_length >= sizeof (context_summary) - strlen ("\n[summary truncated]"))
    {
      summary_length = sizeof (context_summary) - strlen ("\n[summary truncated]") - 1;
      memcpy (context_summary, content->valuestring, summary_length);
      memcpy (context_summary + summary_length, "\n[summary truncated]", strlen ("\n[summary truncated]") + 1);
    }
  else
    memcpy (context_summary, content->valuestring, summary_length + 1);
  cJSON_Delete (parsed);
  summary_active = 1;
  compaction_count++;
  memmove (messages + start, messages + end, (message_count - end) * sizeof (messages[0]));
  message_count -= end - start;
  session_rewrite_compacted ();
  /* Same mid-line hazard as the plain drop: compaction can fire right
     after streamed output or a raw log tail. */
  print_line_break ();
  print_formatted ("[context] Compacted %zu message(s) into the running summary.\n", end - start);
  return (int) (end - start);
}

static int
process_response (const char *body)
{
  cJSON *response;
  cJSON *error;
  cJSON *choices;
  cJSON *choice;
  cJSON *message;
  cJSON *content_item;
  cJSON *tool_calls;
  char content[MAX_LINE];
  ToolCall calls[MAX_TOOL_CALLS];
  size_t call_count = 0;

  response = cJSON_Parse (body);
  if (response == NULL)
    {
      print_formatted ("error: AI response is malformed\n");
      return -1;
    }

  error = cJSON_GetObjectItemCaseSensitive (response, "error");
  if (error != NULL)
    {
      cJSON *message_value = cJSON_GetObjectItemCaseSensitive (error, "message");

      if (copy_json_string (message_value, content, sizeof (content)) < 0)
        print_formatted ("error: AI endpoint returned an error response\n");
      else
        print_formatted ("error: AI endpoint: %s\n", content);
      cJSON_Delete (response);
      return -1;
    }

  choices = cJSON_GetObjectItemCaseSensitive (response, "choices");
  if (!cJSON_IsArray (choices) || cJSON_GetArraySize (choices) < 1)
    {
      print_formatted ("error: AI response has no choice\n");
      cJSON_Delete (response);
      return -1;
    }

  choice = cJSON_GetArrayItem (choices, 0);
  message = cJSON_GetObjectItemCaseSensitive (choice, "message");
  if (!cJSON_IsObject (message))
    {
      print_formatted ("error: AI response has no message\n");
      cJSON_Delete (response);
      return -1;
    }
  record_usage (response);

  content[0] = 0;
  content_item = cJSON_GetObjectItemCaseSensitive (message, "content");
  if (content_item != NULL && !cJSON_IsNull (content_item)
      && copy_json_string (content_item, content, sizeof (content)) < 0)
    {
      print_formatted ("error: assistant content is too long or malformed\n");
      cJSON_Delete (response);
      return -1;
    }

  tool_calls = cJSON_GetObjectItemCaseSensitive (message, "tool_calls");
  if (tool_calls != NULL && !cJSON_IsNull (tool_calls) && !cJSON_IsArray (tool_calls))
    {
      print_formatted ("error: AI response is malformed\n");
      cJSON_Delete (response);
      return -1;
    }
  if (cJSON_IsArray (tool_calls))
    {
      cJSON *item;

      cJSON_ArrayForEach (item, tool_calls)
      {
        if (call_count == MAX_TOOL_CALLS)
          {
            print_formatted ("error: too many tool calls in one response\n");
            cJSON_Delete (response);
            return -1;
          }
        if (parse_tool_call (item, &calls[call_count]) < 0)
          {
            strcpy (calls[call_count].id, "tool-0");
            strcpy (calls[call_count].name, "run_terminal_command");
            calls[call_count].command[0] = 0;
            calls[call_count].supported = 1;
            calls[call_count].valid = 0;
          }
        call_count++;
      }
    }

  if (add_assistant_response (content, calls, call_count) < 0)
    {
      print_formatted ("error: assistant response exceeds conversation limit\n");
      cJSON_Delete (response);
      return -1;
    }
  cJSON_Delete (response);
  if (one_shot)
    print_usage_summary ();
  if (call_count == 0)
    {
      if (!stream_printed_content)
        {
          print_formatted ("\n[AI Response]:\n");
          print_formatted ("%s\n", content[0] != 0 ? content : "(empty response)");
        }
      return 0;
    }
  if (process_tool_calls (calls, call_count) < 0)
    return -1;
  if (one_shot && agent_exit_status != EXIT_SUCCESS)
    return -agent_exit_status;
  return 1;
}

static void
usage (FILE *stream)
{
  fprintf (stream, "Usage: ai [options]\n"
                   "       <command> | ai [options] -e <prompt>\n"
                   "\n"
                   "Options:\n"
                   "-C, --cwd <dir>      set the workspace directory\n"
                   "--full-access        run commands without approval\n"
                   "--no-shell           file tools and plan only; no terminal or job tools\n"
                   "--continue           resume the latest saved session\n"
                   "--resume <file>      resume a specific session file\n"
                   "--ui                 use the line-oriented UI protocol\n"
                   "-f <file>            attach a text file (up to 8 times)\n"
                   "-m, --model <name>   override AI_MODEL for this run\n"
                   "-t, --timeout <sec>  per-command timeout; 0 is unlimited\n"
                   "-e, -exec <prompt>   run one non-empty one-shot prompt\n"
                   "--help               show this help\n"
                   "--version            show version information\n"
                   "\n"
                   "Exit status:\n"
                   "0                    success\n"
                   "1                    runtime, API, or file error\n"
                   "2                    usage error\n"
                   "124                  API or command timeout\n"
                   "130                  cancelled by SIGINT\n"
                   "\n"
                   "Subcommands:\n"
                   "tui                  launch the terminal user interface\n"
                   "editor               launch the standalone multiline editor\n"
                   "\n"
                   "Environment configures AI_API_URL, AI_MODEL, AI_API_KEY or\n"
                   "OPENAI_API_KEY, AI_TIMEOUT_SECONDS and AI_COMMAND_TIMEOUT_SECONDS.\n"
                   "AI_TOOL_ROUNDS is also supported. Set AI_STREAM=0 to buffer responses\n"
                   "instead of streaming them, AI_LANDLOCK=0 to drop the OS-level workspace\n"
                   "sandbox, and NO_COLOR to disable styling.\n");
}

static int
parse_arguments (int argc, char **argv)
{
  int index;

  for (index = 1; index < argc; index++)
    {
      if (strcmp (argv[index], "--full-access") == 0)
        full_access = 1;
      else if (strcmp (argv[index], "--no-shell") == 0)
        no_shell = 1;
      else if (strcmp (argv[index], "--continue") == 0)
        {
          if (session_continue || session_resume_path != NULL)
            {
              fprintf (stderr, "error: --continue may only be supplied once and excludes --resume\n");
              return -1;
            }
          session_continue = 1;
        }
      else if (strcmp (argv[index], "--resume") == 0)
        {
          if (++index >= argc || argv[index][0] == 0)
            {
              fprintf (stderr, "error: --resume requires a session file\n");
              return -1;
            }
          if (session_resume_path != NULL || session_continue)
            {
              fprintf (stderr, "error: --resume may only be supplied once and excludes --continue\n");
              return -1;
            }
          if (strlen (argv[index]) >= sizeof (session_path))
            {
              fprintf (stderr, "error: --resume path must be shorter than %zu bytes\n", sizeof (session_path));
              return -1;
            }
          session_resume_path = argv[index];
        }
      else if (strcmp (argv[index], "--ui") == 0)
        ui_mode = 1;
      else if (strcmp (argv[index], "-m") == 0 || strcmp (argv[index], "--model") == 0)
        {
          if (++index >= argc || argv[index][0] == 0)
            {
              fprintf (stderr, "error: -m requires a model name\n");
              return -1;
            }
          if (strlen (argv[index]) >= sizeof (model))
            {
              fprintf (stderr, "error: -m model name must be shorter than %zu bytes\n", sizeof (model));
              return -1;
            }
          model_override = argv[index];
        }
      else if (strcmp (argv[index], "-t") == 0 || strcmp (argv[index], "--timeout") == 0)
        {
          if (++index >= argc || argv[index][0] == 0)
            {
              fprintf (stderr, "error: -t requires a timeout in seconds\n");
              return -1;
            }
          if (timeout_override >= 0)
            {
              fprintf (stderr, "error: -t may only be supplied once\n");
              return -1;
            }
          timeout_override = parse_timeout_seconds (argv[index], MAX_TIMEOUT);
          if (timeout_override < 0)
            {
              fprintf (stderr, "error: -t must be 0 (unlimited) or at most %d seconds\n", MAX_TIMEOUT);
              return -1;
            }
        }
      else if (strcmp (argv[index], "-C") == 0 || strcmp (argv[index], "--cwd") == 0)
        {
          if (++index >= argc || argv[index][0] == 0)
            {
              fprintf (stderr, "error: -C requires a directory\n");
              return -1;
            }
          if (workspace_directory != NULL)
            {
              fprintf (stderr, "error: -C may only be supplied once\n");
              return -1;
            }
          workspace_directory = argv[index];
        }
      else if (strcmp (argv[index], "-f") == 0)
        {
          if (++index >= argc || argv[index][0] == 0)
            {
              fprintf (stderr, "error: -f requires a file path\n");
              return -1;
            }
          if (attachment_count == MAX_ATTACHMENTS)
            {
              fprintf (stderr, "error: at most %d files may be attached\n", MAX_ATTACHMENTS);
              return -1;
            }
          attachment_paths[attachment_count++] = argv[index];
        }
      else if (strcmp (argv[index], "-exec") == 0 || strcmp (argv[index], "-e") == 0)
        {
          if (exec_prompt != NULL)
            {
              usage (stderr);
              return -1;
            }
          if (++index >= argc || argv[index][0] == 0)
            {
              fprintf (stderr, "error: -exec requires a non-empty prompt\n");
              return -1;
            }
          if (strlen (argv[index]) >= MAX_COMMAND)
            {
              fprintf (stderr, "error: -exec prompt must be shorter than %d bytes\n", MAX_COMMAND);
              return -1;
            }
          exec_prompt = argv[index];
        }
      else if (strcmp (argv[index], "--help") == 0)
        {
          usage (stdout);
          exit (0);
        }
      else if (strcmp (argv[index], "--version") == 0)
        {
          print_formatted ("ai %s %s %s\n", VERSION, BUILD_DATE, BUILD_REVISION);
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

int
ai_main (int argc, char **argv)
{
  static char response_storage[MAX_RESPONSE];
  static char prompt[MAX_INITIAL_PROMPT];
  const char *response_start = NULL;
  struct sigaction action;

  if (parse_arguments (argc, argv) < 0)
    return 2;
  {
    const char *term = getenv ("TERM");

    terminal_styling = !ui_mode && isatty (STDOUT_FILENO) && getenv ("NO_COLOR") == NULL && term != NULL && term[0] != 0
                       && strcmp (term, "dumb") != 0;
  }
  editor_set_prompt_printer (print_editor_prompt);
  editor_set_interrupt_checker (interrupt_is_pending);
  if (configure_environment () < 0)
    return 2;
  if (workspace_directory != NULL && realpath (workspace_directory, workspace_root) == NULL)
    {
      fprintf (stderr, "error: cannot resolve workspace directory: %s\n", strerror (errno));
      return 2;
    }
  if (workspace_directory != NULL && chdir (workspace_root) < 0)
    {
      fprintf (stderr, "error: cannot enter %s: %s\n", workspace_directory, strerror (errno));
      return 2;
    }
  if (getcwd (current_directory, sizeof (current_directory)) == NULL)
    snprintf (current_directory, sizeof (current_directory), ".");
  one_shot = exec_prompt != NULL;
  if (one_shot)
    {
      Buffer initial_prompt = { prompt, sizeof (prompt), 0 };

      if (build_one_shot_prompt (exec_prompt, &initial_prompt) < 0)
        return 1;
    }
  hide_control_characters ();
  /* Registered after hide_control_characters, so atexit's LIFO order stops
     jobs before the terminal state is restored. */
  atexit (jobs_shutdown);
  memset (&action, 0, sizeof (action));
  action.sa_handler = on_signal;
  sigemptyset (&action.sa_mask);
  sigaddset (&action.sa_mask, SIGINT);
  sigaction (SIGINT, &action, NULL);
  sigaction (SIGTERM, &action, NULL);
  signal (SIGPIPE, SIG_IGN);
  load_skills ();
  conversation_reset ();
  if (!one_shot)
    {
      print_formatted ("ai %s %s %s interactive AI agent\n", VERSION, BUILD_DATE, BUILD_REVISION);
      print_formatted ("Endpoint: http://%s:%d%s\n", api_host, api_port, api_path);
      {
        char timeout_text[16];

        snprintf (timeout_text, sizeof (timeout_text), "%ds", command_timeout);
        print_formatted ("Model: %s; API timeout: %ds; command timeout: %s; tool rounds: %d\n", model, api_timeout,
                         command_timeout > 0 ? timeout_text : "none", max_tool_rounds);
      }
      print_formatted ("Workspace: %s (%s)\n", workspace_directory != NULL ? workspace_root : current_directory,
                       workspace_directory != NULL ? "scoped" : "unrestricted");
      if (workspace_directory != NULL && !full_access)
        {
          if (!landlock_enabled)
            print_formatted ("Sandbox: lexical path filter (landlock disabled)\n");
          else if (landlock_supported () > 0)
            print_formatted ("Sandbox: landlock rules + lexical path filter\n");
          else
            print_formatted ("Sandbox: lexical path filter (landlock unavailable on this kernel)\n");
        }
    }
  if (skill_count > 0)
    {
      char listing[MAX_SKILLS * (MAX_SKILL_NAME + 2) + 16];
      Buffer buffer = { listing, sizeof (listing), 0 };
      int index;

      for (index = 0; index < skill_count; index++)
        if (buffer_append_formatted (&buffer, "%s%s", index == 0 ? "" : ", ", skills[index].name) < 0)
          break;
      if (buffer.length < sizeof (listing))
        listing[buffer.length] = 0;
      else
        listing[sizeof (listing) - 1] = 0;
      print_formatted ("Skills: %s\n", listing);
    }
  if (!one_shot)
    {
      print_formatted (multiline_keys_active ()
                          ? "Multiline: C-J newline, Enter submit. /help shows commands.\n"
                          : "Multiline input ends with an empty line. /help shows commands.\n");
      print_formatted ("Live steering: type lines while AI requests or commands run.\n");
      if (no_shell)
        print_formatted ("Shell tools disabled by --no-shell: file tools and plan only.\n");
      else
        print_formatted ("Shell mode: ! <command> runs it here; its output joins the conversation.\n");
      if (full_access)
        print_formatted ("Full access: commands run immediately; steering is offered after each one.\n");
      else
        print_formatted ("Every command is shown for allow/edit/steer/force/refuse before execution.\n");
    }
  snprintf (session_path, sizeof (session_path), "%s", ".ai/session.jsonl");
  if (session_resume_path != NULL)
    snprintf (session_path, sizeof (session_path), "%s", session_resume_path);
  if (session_continue || session_resume_path != NULL)
    {
      if (resume_session (session_path) < 0)
        {
          if (session_resume_path != NULL)
            {
              fprintf (stderr, "error: cannot read session %s: %s\n", session_resume_path, strerror (errno));
              return 2;
            }
          print_formatted ("[session] no previous session in %s; starting fresh\n", session_path);
        }
    }
  for (;;)
    {
      /* A signal that lands between the prompt draw and the blocking read
         would otherwise be swallowed; Ctrl+C at an idle prompt still exits. */
      if (!one_shot && interrupted)
        break;
      if (one_shot)
        ;
      else if (read_multiline ("\n[You]: ", "[...]: ", prompt, sizeof (prompt)) < 0)
        break;
      if (prompt[0] == 0)
        continue;
      if (!one_shot && attachment_count != 0)
        {
          char combined[MAX_INITIAL_PROMPT];
          Buffer initial_prompt = { combined, sizeof (combined), 0 };

          if (build_prompt_with_attachments (prompt, &initial_prompt) < 0)
            break;
          memcpy (prompt, combined, sizeof (prompt));
        }
      if (!one_shot && prompt[0] == '!')
        {
          if (no_shell)
            print_formatted ("error: shell commands are disabled by --no-shell\n");
          else
            run_shell_prompt (prompt + 1);
          continue;
        }
      if (!one_shot && (strcmp (prompt, "/exit") == 0 || strcmp (prompt, "/quit") == 0))
        break;
      if (!one_shot && strcmp (prompt, "/new") == 0)
        {
          conversation_reset ();
          clear_steering ();
          session_restart ();
          print_formatted ("[context reset]\n");
          continue;
        }
      if (!one_shot && strcmp (prompt, "/force") == 0)
        {
          full_access = !full_access;
          print_formatted ("[force mode is now %s]\n", full_access ? "ON; commands run without approval" : "OFF");
          continue;
        }
      if (!one_shot && strcmp (prompt, "/skills") == 0)
        {
          int index;

          if (skill_count == 0)
            print_formatted ("No skills found under " SKILLS_DIR ".\n");
          for (index = 0; index < skill_count; index++)
            print_formatted ("%s: %s\n", skills[index].name, skills[index].description);
          continue;
        }
      if (!one_shot && strncmp (prompt, "/skill", 6) == 0)
        {
          if (prompt[6] == ' ')
            handle_skill_command (prompt + 7);
          else
            print_formatted ("usage: /skill <name>; /skills lists them\n");
          continue;
        }
      if (!one_shot && strcmp (prompt, "/compact") == 0)
        {
          size_t start = 0;
          size_t end;

          /* Fold the older half of the conversation, between subtasks:
             system messages stay, groups stay whole, and enough recent
             context remains that the next request still has a user turn. */
          while (start < message_count && messages[start].role == ROLE_SYSTEM)
            start++;
          end = start + (message_count - start) / 2;
          while (end < message_count && messages[end].role == ROLE_TOOL)
            end++;
          {
            int kept_user = 0;
            size_t index;

            for (index = end; index < message_count; index++)
              if (messages[index].role == ROLE_USER)
                kept_user = 1;
            if (!kept_user)
              end = start;
          }
          if (end <= start)
            print_formatted ("[context] Too little conversation to compact safely yet.\n");
          else if (compact_messages (start, end) > 0)
            compaction_round = response_rounds;
          continue;
        }
      if (!one_shot && strncmp (prompt, "/model", 6) == 0 && (prompt[6] == 0 || prompt[6] == ' '))
        {
          char *name = prompt + 6;
          size_t name_length;

          /* Bare /model reports; a name switches from the next request on.
             The conversation keeps its history: only the request header
             changes, so a mid-session model swap costs no context. */
          trim_spaces (name);
          name_length = strlen (name);
          if (name_length == 0)
            print_formatted ("[model: %s]\n", model);
          else if (name_length >= sizeof (model))
            print_formatted ("error: model name must be shorter than %zu bytes\n", sizeof (model));
          else
            {
              memcpy (model, name, name_length + 1);
              print_formatted ("[model is now %s]\n", model);
            }
          continue;
        }
      if (!one_shot && strcmp (prompt, "/help") == 0)
        {
          print_formatted ("Commands:\n"
                           "/new            reset context\n"
                           "/compact        fold old context into a summary\n"
                           "/force          no need allow\n"
                           "/model <name>   switch model\n"
                           "/skills         list skills\n"
                           "/skill <name>   load a skill\n"
                           "! <command>     run shell command\n"
                           "/exit, /quit    exit\n");
          continue;
        }
      if (add_text_message ("user", prompt, 0) < 0)
        {
          print_formatted ("error: prompt cannot fit in the conversation\n");
          continue;
        }
      if (!one_shot)
        attachment_count = 0;
      response_start = NULL;
      reap_jobs ();
      if (call_endpoint_threaded (&response_start, response_storage) < 0)
        {
          size_t old_count = message_count;
          int shed_prompt = 0;

          drop_oldest_message ();
          if (old_count == message_count && message_count > 0
              && messages[message_count - 1].role == ROLE_USER)
            {
              /* Nothing older can go; forget this unsent prompt so the
                 conversation stays sendable for the next turn. */
              message_count--;
              shed_prompt = 1;
            }
          if ((old_count == message_count && !shed_prompt) || one_shot)
            {
              if (agent_exit_status == EXIT_SUCCESS)
                return 1;
              return one_shot ? agent_exit_status : 1;
            }
          continue;
        }
      {
        int result;
        int round;
        int pending_round = 0;

        result = process_response (response_start);
        if (result < 0)
          {
            if (agent_exit_status == EXIT_SUCCESS)
              return 1;
            return one_shot ? agent_exit_status : 1;
          }
        pending_round = result;
        for (round = 1; result == 1; round++)
          {
            if (round == max_tool_rounds)
              {
                print_formatted ("\n[AI Response]: Stopped after %d tool rounds; raise AI_TOOL_ROUNDS for longer tasks.\n",
                                 max_tool_rounds);
                result = 0;
                break;
              }
            response_start = NULL;
            reap_jobs ();
            if (call_endpoint_threaded (&response_start, response_storage) < 0)
              {
                size_t old_count = message_count;

                drop_oldest_message ();
                if (old_count == message_count || one_shot)
                  {
                    if (agent_exit_status == EXIT_SUCCESS)
                      return 1;
                    return one_shot ? agent_exit_status : 1;
                  }
                round--;
                continue;
              }
            result = process_response (response_start);
            if (result < 0)
              {
                if (agent_exit_status == EXIT_SUCCESS)
                  return 1;
                return one_shot ? agent_exit_status : 1;
              }
          }
        if (pending_round == 0 && add_pending_request_steering () < 0)
          return 1;
      }
      if (one_shot)
        break;
    }
  if (!one_shot)
    {
      print_usage_summary ();
      print_formatted ("\n[bye]\n");
    }
  return 0;
}
