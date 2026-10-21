#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <sys/select.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "version.h"

#include "cJSON.h"

#define VERSION "1.0"
#define DEFAULT_ENDPOINT "http://127.0.0.1:8090/v1/chat/completions"
#define DEFAULT_MODEL "glm-5.3-flash"
#define SYSTEM_PROMPT \
  "You are a senior software engineer working directly on the user's host. " \
  "Diagnose methodically, state assumptions and impact, prefer read-only " \
  "commands first, and finish with a concise conclusion. Request exactly one " \
  "terminal command per tool call. Never conceal or misrepresent command " \
  "output. Stop and ask the human when an action is destructive, ambiguous, " \
  "or outside the stated task."

#define MAX_ENDPOINT 512
#define MAX_HOST 128
#define MAX_PATH 384
#define MAX_KEY 512
#define MAX_MODEL 128
#define MAX_LINE 8192
#define MAX_COMMAND 8192
#define MAX_ID 256
#define MAX_NAME 128
#define MAX_TOOL_CALLS 8
#define MAX_MESSAGES 24
#define MAX_TOOL_ROUNDS 8
#define MESSAGE_SIZE 12288
#define MAX_REQUEST (512 * 1024)
#define MAX_RESPONSE (256 * 1024)
#define CAPTURE_SIZE 16384
#define MODEL_OUTPUT_SIZE 3072
#define MAX_TIMEOUT 600
#define MAX_STEERING_LINES 64

typedef struct {
  char json[MESSAGE_SIZE];
  size_t length;
  int system;
} Message;

typedef struct {
  char *data;
  size_t capacity;
  size_t length;
} Buffer;

typedef struct {
  char id[MAX_ID];
  char name[MAX_NAME];
  char command[MAX_COMMAND];
  int supported;
  int valid;
} ToolCall;

static Message messages[MAX_MESSAGES];
static size_t message_count;
static char endpoint[MAX_ENDPOINT] = DEFAULT_ENDPOINT;
static char api_host[MAX_HOST] = "127.0.0.1";
static char api_path[MAX_PATH] = "/v1/chat/completions";
static int api_port = 8090;
static char api_key[MAX_KEY];
static char model[MAX_MODEL] = DEFAULT_MODEL;
static int api_timeout = 60;
static int command_timeout = 60;
static int full_access;
static volatile sig_atomic_t interrupted;
static int ui_mode;

typedef struct {
  char text[MAX_COMMAND];
  int complete;
} SteeringLine;

static SteeringLine steering_input[MAX_STEERING_LINES];
static size_t steering_read_index;
static size_t steering_write_index;
static pthread_mutex_t steering_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
  struct termios original_terminal;
  char line[MAX_COMMAND];
  size_t line_length;
  int input_flags;
  int terminal_configured;
  int input_open;
} LiveInput;

static LiveInput live_input;

static void on_signal(int signal_number)
{
  (void) signal_number;
  interrupted = 1;
}

static int take_steering(char *buffer, size_t capacity)
{
  int available = 0;

  pthread_mutex_lock(&steering_mutex);
  if (steering_input[steering_read_index].complete)
    {
      SteeringLine *line = &steering_input[steering_read_index];

      if (strlen(line->text) < capacity)
        {
          strcpy(buffer, line->text);
          available = 1;
        }
      line->complete = 0;
      steering_read_index =
        (steering_read_index + 1) % MAX_STEERING_LINES;
    }
  pthread_mutex_unlock(&steering_mutex);
  return available;
}

static int append_live_steering(char *steer, size_t capacity)
{
  char line[MAX_COMMAND];
  size_t used;
  int appended = 0;

  if (capacity == 0)
    return -1;
  used = strlen(steer);
  while (take_steering(line, sizeof(line)))
    {
      size_t line_length = strlen(line);

      if (line_length == 0)
        continue;
      if (used != 0 && used + 1 >= capacity)
        return -1;
      if (line_length >= capacity - used - (used != 0 ? 1 : 0))
        return -1;
      if (used != 0)
        steer[used++] = '\n';
      memcpy(steer + used, line, line_length + 1);
      used += line_length;
      appended = 1;
    }
  return appended;
}

static int write_all(int fd, const char *data, size_t length)
{
  while (length > 0)
    {
      ssize_t written = write(fd, data, length);
      if (written < 0)
        {
          if (errno == EINTR)
            continue;
          return -1;
        }
      if (written == 0)
        return -1;
      data += written;
      length -= (size_t) written;
    }
  return 0;
}

static void print_formatted(const char *format, ...)
{
  char text[MAX_LINE];
  va_list arguments;
  int length;

  va_start(arguments, format);
  length = vsnprintf(text, sizeof(text), format, arguments);
  va_end(arguments);
  if (length < 0)
    return;
  if (write_all(STDOUT_FILENO, text, (size_t) length) < 0)
    {
      /* Output failures are not recoverable in this compact tool. */
    }
}

static void live_steering_prompt(void)
{
  print_formatted("[live steering active]: type one line per direction\n");
}

static void queue_live_line(const char *text, size_t length)
{
  SteeringLine *slot;
  int queued = 0;

  pthread_mutex_lock(&steering_mutex);
  slot = &steering_input[steering_write_index];
  if (!slot->complete)
    {
      memcpy(slot->text, text, length + 1);
      slot->complete = 1;
      steering_write_index = (steering_write_index + 1)
                             % MAX_STEERING_LINES;
      queued = 1;
    }
  pthread_mutex_unlock(&steering_mutex);
  if (queued)
    print_formatted("[live steering queued]: %s\n", text);
  else
    print_formatted("[live steering dropped]: steering queue is full\n");
}

static void erase_live_input(size_t length)
{
  if (length == 0 || length > live_input.line_length)
    return;
  live_input.line_length -= length;
  live_input.line[live_input.line_length] = 0;
  if (length == 1)
    print_formatted("\b \b");
  else
    print_formatted("\r\33[K%s", live_input.line);
}

static int begin_live_input(void)
{
  struct termios terminal;
  int flags;

  live_input.line_length = 0;
  live_input.terminal_configured = 0;
  flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  live_input.input_open = flags >= 0;
  if (!live_input.input_open)
    return 0;
  live_input.input_flags = flags;
  if (fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK) < 0)
    {
      live_input.input_open = 0;
      return 0;
    }
  if (tcgetattr(STDIN_FILENO, &live_input.original_terminal) != 0)
    return 0;
  terminal = live_input.original_terminal;
  terminal.c_lflag &= ~ICANON;
  terminal.c_cc[VMIN] = 0;
  terminal.c_cc[VTIME] = 0;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &terminal) != 0)
    return 0;
  live_input.terminal_configured = 1;
  return 0;
}

static void poll_live_input(void)
{
  char character;

  if (!live_input.input_open)
    return;
  while (read(STDIN_FILENO, &character, 1) == 1)
    {
      if (character == '\n' || character == '\r')
        {
          if (live_input.line_length != 0)
            {
              live_input.line[live_input.line_length] = 0;
              queue_live_line(live_input.line, live_input.line_length);
              live_input.line_length = 0;
            }
        }
      else if (character == 127 || character == 8)
        erase_live_input(1);
      else if (character == 4)
        erase_live_input(live_input.line_length);
      else if (character == 21)
        {
          size_t start = live_input.line_length;

          while (start != 0 && live_input.line[start - 1] == ' ')
            start--;
          while (start != 0 && live_input.line[start - 1] != ' ')
            start--;
          erase_live_input(live_input.line_length - start);
        }
      else if (live_input.line_length < sizeof(live_input.line) - 1)
        live_input.line[live_input.line_length++] = character;
    }
}

static void end_live_input(void)
{
  if (live_input.line_length != 0)
    {
      live_input.line[live_input.line_length] = 0;
      queue_live_line(live_input.line, live_input.line_length);
      live_input.line_length = 0;
    }
  if (live_input.terminal_configured)
    tcsetattr(STDIN_FILENO, TCSANOW, &live_input.original_terminal);
  fcntl(STDIN_FILENO, F_SETFL, live_input.input_flags);
  live_input.terminal_configured = 0;
  live_input.input_open = 0;
}

static int steering_available(void)
{
  int available;

  pthread_mutex_lock(&steering_mutex);
  available = steering_input[steering_read_index].complete;
  pthread_mutex_unlock(&steering_mutex);
  return available;
}

static void clear_steering(void)
{
  pthread_mutex_lock(&steering_mutex);
  memset(steering_input, 0, sizeof(steering_input));
  steering_read_index = 0;
  steering_write_index = 0;
  pthread_mutex_unlock(&steering_mutex);
}

static int buffer_append(Buffer *buffer, const char *data, size_t length)
{
  if (length > buffer->capacity - buffer->length)
    return -1;
  memcpy(buffer->data + buffer->length, data, length);
  buffer->length += length;
  return 0;
}

static int buffer_append_string(Buffer *buffer, const char *data)
{
  return buffer_append(buffer, data, strlen(data));
}

__attribute__((format(printf, 2, 3)))
static int buffer_append_formatted(Buffer *buffer, const char *format, ...)
{
  char text[MAX_LINE];
  va_list arguments;
  int length;

  va_start(arguments, format);
  length = vsnprintf(text, sizeof(text), format, arguments);
  va_end(arguments);
  if (length < 0)
    return -1;
  return buffer_append(buffer, text, (size_t) length);
}

static int append_json_string_body(Buffer *buffer, const char *data,
                                   size_t length)
{
  size_t index;

  for (index = 0; index < length; index++)
    {
      unsigned char byte = (unsigned char) data[index];
      char escaped[7];

      if (byte == '"' || byte == '\\')
        {
          escaped[0] = '\\';
          escaped[1] = (char) byte;
          if (buffer_append(buffer, escaped, 2) < 0)
            return -1;
        }
      else if (byte == '\b' || byte == '\f' || byte == '\n'
               || byte == '\r' || byte == '\t')
        {
          escaped[0] = '\\';
          escaped[1] = (char) (
            byte == '\b' ? 'b' :
            byte == '\f' ? 'f' :
            byte == '\n' ? 'n' :
            byte == '\r' ? 'r' : 't');
          if (buffer_append(buffer, escaped, 2) < 0)
            return -1;
        }
      else if (byte < 0x20)
        {
          snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned int) byte);
          if (buffer_append(buffer, escaped, 6) < 0)
            return -1;
        }
      else if (buffer_append(buffer, (const char *) &data[index], 1) < 0)
        return -1;
    }
  return 0;
}

static int append_json_string(Buffer *buffer, const char *data, size_t length)
{
  if (buffer_append_string(buffer, "\"") < 0
      || append_json_string_body(buffer, data, length) < 0
      || buffer_append_string(buffer, "\"") < 0)
    return -1;
  return 0;
}

static int append_json_text(Buffer *buffer, const char *data)
{
  return append_json_string(buffer, data, strlen(data));
}

static void drop_oldest_message(void)
{
  size_t index;

  for (index = 1; index < message_count; index++)
    if (!messages[index].system)
      break;
  if (index == message_count)
    return;
  memmove(messages + index, messages + index + 1,
          (message_count - index - 1) * sizeof(messages[0]));
  message_count--;
  print_formatted("[context] Dropped the oldest non-system message.\n");
}

static int add_message(const char *json, size_t length, int system)
{
  while (message_count == MAX_MESSAGES || length > MESSAGE_SIZE)
    {
      size_t old_count = message_count;
      drop_oldest_message();
      if (old_count == message_count)
        return -1;
    }
  memcpy(messages[message_count].json, json, length);
  messages[message_count].length = length;
  messages[message_count].system = system;
  message_count++;
  return 0;
}

static int add_text_message(const char *role, const char *content, int system)
{
  char json[MESSAGE_SIZE];
  Buffer buffer = {json, sizeof(json), 0};

  if (buffer_append_formatted(&buffer, "{\"role\":") < 0
      || append_json_text(&buffer, role) < 0
      || buffer_append_string(&buffer, ",\"content\":") < 0
      || append_json_text(&buffer, content) < 0
      || buffer_append_string(&buffer, "}") < 0)
    return -1;
  return add_message(json, buffer.length, system);
}

static void conversation_reset(void)
{
  message_count = 0;
  if (add_text_message("system", SYSTEM_PROMPT, 1) < 0)
    {
      print_formatted("error: unable to initialize conversation\n");
      exit(1);
    }
}

static int parse_positive_integer(const char *text, int maximum)
{
  char *end;
  long value;
  int conversion_error;

  errno = 0;
  value = strtol(text, &end, 10);
  conversion_error = errno == ERANGE;
  if (conversion_error || end == text || *end != 0 || value <= 0
      || value > maximum)
    return -1;
  return (int) value;
}

static int valid_ipv4(const char *host, size_t length)
{
  char copy[MAX_HOST];
  unsigned int parts[4];
  char *cursor;
  char *part;
  int index;

  if (length == 0 || length >= sizeof(copy))
    return 0;
  memcpy(copy, host, length);
  copy[length] = 0;
  cursor = copy;
  for (index = 0; index < 4; index++)
    {
      char *end;
      unsigned long value;
      int conversion_error;

      part = strtok(index == 0 ? cursor : NULL, ".");
      if (part == NULL || part[strspn(part, "0123456789")] != 0)
        return 0;
      errno = 0;
      value = strtoul(part, &end, 10);
      conversion_error = errno == ERANGE;
      if (conversion_error || *end != 0 || value > 255
          || (part[0] == '0' && part[1] != 0))
        return 0;
      parts[index] = (unsigned int) value;
    }
  if (strtok(NULL, ".") != NULL)
    return 0;
  return parts[0] != 0;
}

static int configure_endpoint(const char *value)
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

  if (value == NULL || strncmp(value, "http://", 7) != 0)
    {
      print_formatted("error: AI_API_URL must be an http:// IPv4 URL\n");
      return -1;
    }
  host = value + 7;
  slash = strchr(host, '/');
  end = slash != NULL ? slash : host + strlen(host);
  port = memchr(host, ':', (size_t) (end - host));
  if (port != NULL)
    {
      host_length = (size_t) (port - host);
      port++;
      errno = 0;
      number = strtol(port, &parse_end, 10);
      conversion_error = errno == ERANGE;
      if (parse_end != end || conversion_error || number <= 0
          || number > 65535)
        {
          print_formatted("error: AI_API_URL port is invalid\n");
          return -1;
        }
      api_port = (int) number;
    }
  else
    {
      host_length = (size_t) (end - host);
      api_port = 80;
    }
  if (host_length == 0 || host_length >= sizeof(api_host))
    {
      print_formatted("error: AI_API_URL host is too long\n");
      return -1;
    }
  memcpy(api_host, host, host_length);
  api_host[host_length] = 0;
  if (strcmp(api_host, "localhost") != 0
      && !valid_ipv4(api_host, host_length))
    {
      print_formatted("error: endpoint host must be an IPv4 address or localhost\n");
      return -1;
    }
  if (strcmp(api_host, "localhost") == 0)
    strcpy(api_host, "127.0.0.1");
  path_length = slash != NULL ? strlen(slash) : 0;
  if (path_length == 0)
    strcpy(api_path, "/v1/chat/completions");
  else if (path_length >= sizeof(api_path))
    {
      print_formatted("error: AI_API_URL path is too long\n");
      return -1;
    }
  else
    strcpy(api_path, slash);
  if (strcmp(api_path, "/") == 0)
    strcpy(api_path, "/v1/chat/completions");
  else if (strcmp(api_path, "/v1") == 0)
    {
      if (path_length + strlen("chat/completions") >= sizeof(api_path))
        {
          print_formatted("error: AI_API_URL path is too long\n");
          return -1;
        }
      if (api_path[path_length - 1] == '/')
        memcpy(api_path + path_length, "chat/completions", 17);
      else
        memcpy(api_path + path_length, "/chat/completions", 18);
    }
  else if (path_length > 1 && api_path[path_length - 1] == '/')
    {
      if (path_length + strlen("chat/completions") >= sizeof(api_path))
        {
          print_formatted("error: AI_API_URL path is too long\n");
          return -1;
        }
      memcpy(api_path + path_length, "chat/completions", 17);
    }
  if (strlen(value) >= sizeof(endpoint))
    {
      print_formatted("error: AI_API_URL is too long\n");
      return -1;
    }
  strcpy(endpoint, value);
  return 0;
}

static int valid_header_value(const char *value)
{
  size_t index;

  for (index = 0; value[index] != 0; index++)
    if (value[index] < 0x21 || value[index] > 0x7e)
      return 0;
  return 1;
}

static int configure_environment(void)
{
  const char *value;
  const char *selected_key;
  int timeout;

  value = getenv("AI_API_URL");
  if (value == NULL)
    value = DEFAULT_ENDPOINT;
  if (configure_endpoint(value) < 0)
    return -1;
  value = getenv("AI_MODEL");
  if (value != NULL)
    {
      if (value[0] == 0 || strlen(value) >= sizeof(model))
        {
          print_formatted("error: AI_MODEL is invalid\n");
          return -1;
        }
      strcpy(model, value);
    }
  selected_key = getenv("AI_API_KEY");
  if (selected_key == NULL)
    selected_key = getenv("OPENAI_API_KEY");
  if (selected_key != NULL)
    {
      if (strlen(selected_key) >= sizeof(api_key)
          || !valid_header_value(selected_key))
        {
          print_formatted("error: API key is invalid\n");
          return -1;
        }
      strcpy(api_key, selected_key);
    }
  value = getenv("AI_TIMEOUT_SECONDS");
  if (value != NULL)
    {
      timeout = parse_positive_integer(value, MAX_TIMEOUT);
      if (timeout < 0)
        {
          print_formatted("error: AI_TIMEOUT_SECONDS is invalid\n");
          return -1;
        }
      api_timeout = timeout;
    }
  value = getenv("AI_COMMAND_TIMEOUT_SECONDS");
  if (value != NULL)
    {
      timeout = parse_positive_integer(value, MAX_TIMEOUT);
      if (timeout < 0)
        {
          print_formatted("error: AI_COMMAND_TIMEOUT_SECONDS is invalid\n");
          return -1;
        }
      command_timeout = timeout;
    }
  return 0;
}

static int copy_json_string(const cJSON *item, char *output,
                            size_t capacity)
{
  const char *value;

  if (!cJSON_IsString(item) || capacity == 0)
    return -1;
  value = cJSON_GetStringValue(item);
  if (strlen(value) >= capacity)
    return -1;
  strcpy(output, value);
  return 0;
}

static int header_equal(const char *line, size_t length, const char *name)
{
  size_t name_length = strlen(name);

  return length > name_length && strncasecmp(line, name, name_length) == 0
         && line[name_length] == ':';
}

static int receive_response(int socket_fd, char *response, size_t capacity,
                            size_t *response_length, char **body,
                            int *error_number)
{
  time_t deadline;
  fd_set read_set;
  size_t total = 0;
  char *header_end = NULL;
  size_t content_length = 0;
  int have_content_length = 0;
  int chunked = 0;
  int status;
  char *line;

  *error_number = EIO;
  deadline = time(NULL) + api_timeout;
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
      FD_ZERO(&read_set);
      FD_SET(socket_fd, &read_set);
      remaining = deadline - time(NULL);
      if (remaining <= 0)
        {
          *error_number = ETIMEDOUT;
          return -1;
        }
      {
        struct timeval timeout = {remaining, 0};

        ready = select(socket_fd + 1, &read_set, NULL, NULL, &timeout);
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
          return -1;
        }
      received = recv(socket_fd, response + total, capacity - 1 - total, 0);
      if (received < 0)
        {
          if (errno == EINTR && !interrupted)
            continue;
          *error_number = errno;
          return -1;
        }
      if (received == 0)
        break;
      total += (size_t) received;
      response[total] = 0;
      if (header_end == NULL)
        {
          header_end = strstr(response, "\r\n\r\n");
          if (header_end != NULL)
            {
              int code = 0;

              if (sscanf(response, "HTTP/%*d.%*d %d", &code) != 1)
                {
                  *error_number = EPROTO;
                  return -1;
                }
              status = code;
              *body = header_end + 4;
            }
        }
      if (header_end != NULL)
        {
          line = response;
          while (line < header_end && line[0] != '\r')
            {
              char *next = memchr(line, '\r', (size_t) (header_end - line));
              size_t length;

              if (next == NULL)
                next = header_end;
              length = (size_t) (next - line);

              if (header_equal(line, length, "Content-Length"))
                {
                  char *end;
                  const char *value_start;
                  unsigned long value;
                  int conversion_error;

                  value_start = line + 15;
                  while (value_start < next && (*value_start == ' '
                                                 || *value_start == '\t'))
                    value_start++;
                  errno = 0;
                  value = strtoul(value_start, &end, 10);
                  conversion_error = errno == ERANGE;
                  if (conversion_error || end > next || end == value_start
                      || value > MAX_RESPONSE)
                    {
                      *error_number = conversion_error ? ERANGE : EPROTO;
                      return -1;
                    }
                  content_length = (size_t) value;
                  have_content_length = 1;
                }
              else if (header_equal(line, length, "Transfer-Encoding")
                       && length > 19
                       && strncasecmp(line + 18, " chunked", 8) == 0)
                chunked = 1;
              line = next + 2;
            }
          if (have_content_length
              && total >= (size_t) (*body - response) + content_length)
            break;
          if (chunked && strstr(response, "\r\n0\r\n") != NULL)
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
      print_formatted("error: AI endpoint returned HTTP %d\n", status);
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
          chunk = strtoul(source, &next, 16);
          conversion_error = errno == ERANGE;
          if (conversion_error || next == source
              || (*next != '\r' && *next != ';'))
            {
              *error_number = conversion_error ? ERANGE : EPROTO;
              return -1;
            }
          source = memchr(next, '\n', (size_t) (response + total - next));
          if (source == NULL)
            {
              *error_number = EPROTO;
              return -1;
            }
          source++;
          if (chunk == 0)
            break;
          if (chunk > (size_t) (response + total - source))
            {
              *error_number = EPROTO;
              return -1;
            }
          memmove(destination, source, chunk);
          destination += chunk;
          source += chunk;
          if (response + total - source < 2 || source[0] != '\r'
              || source[1] != '\n')
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
      *response_length = (size_t) (destination - original_body);
      *body = original_body;
    }
  else if (have_content_length)
    *response_length = content_length;
  else
    *response_length = total - (size_t) (*body - response);
  (*body)[*response_length] = 0;
  return 0;
}

static int connect_endpoint(void)
{
  struct sockaddr_in address;
  int socket_fd;
  struct timeval timeout = {api_timeout, 0};
  fd_set set;
  int result;
  int error;

  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_port = htons((uint16_t) api_port);
  if (inet_pton(AF_INET, api_host, &address.sin_addr) != 1)
    {
      errno = EINVAL;
      return -1;
    }
  socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (socket_fd < 0)
    return -1;
  if (fcntl(socket_fd, F_SETFL, O_NONBLOCK) < 0)
    {
      close(socket_fd);
      return -1;
    }
  result = connect(socket_fd, (struct sockaddr *) &address, sizeof(address));
  if (result < 0)
    {
      if (errno != EINPROGRESS)
        {
          close(socket_fd);
          return -1;
        }
      FD_ZERO(&set);
      FD_SET(socket_fd, &set);
      result = select(socket_fd + 1, NULL, &set, NULL, &timeout);
      if (result <= 0)
        {
          close(socket_fd);
          errno = result == 0 ? ETIMEDOUT : EINTR;
          return -1;
        }
      socklen_t error_size = sizeof(error);

      error = 0;
      if (getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &error, &error_size) < 0
          || error != 0)
        {
          close(socket_fd);
          errno = error != 0 ? error : EIO;
          return -1;
        }
    }
  if (fcntl(socket_fd, F_SETFL, 0) < 0)
    {
      close(socket_fd);
      return -1;
    }
  return socket_fd;
}

static int build_request(char *request, size_t capacity, size_t *length)
{
  Buffer buffer = {request, capacity, 0};
  size_t index;

  if (buffer_append_string(&buffer,
      "{\"model\":") < 0
      || append_json_text(&buffer, model) < 0
      || buffer_append_string(&buffer,
          ",\"messages\":[") < 0)
    return -1;
  for (index = 0; index < message_count; index++)
    {
      if (index != 0 && buffer_append_string(&buffer, ",") < 0)
        return -1;
      if (buffer_append(&buffer, messages[index].json,
                        messages[index].length) < 0)
        return -1;
    }
  if (buffer_append_string(&buffer,
      "],\"tools\":[{\"type\":\"function\",\"function\":{"
      "\"name\":\"run_terminal_command\",\"description\":"
      "\"Execute one shell command on the user's host and return bounded live output.\","
      "\"parameters\":{\"type\":\"object\",\"properties\":{"
      "\"command\":{\"type\":\"string\",\"description\":"
      "\"The complete shell command to execute.\"}},"
      "\"required\":[\"command\"]}}}],\"tool_choice\":\"auto\"}") < 0)
    return -1;
  *length = buffer.length;
  return 0;
}

static int call_endpoint(const char **response_body, char *response_storage)
{
  static char request[MAX_REQUEST];
  size_t request_length;
  char headers[1024];
  int header_length;
  int socket_fd;
  int receive_status;
  int receive_error;
  size_t response_length;
  char *body;

  response_length = 0;
  body = NULL;
  if (build_request(request, sizeof(request), &request_length) < 0)
    {
      print_formatted("error: conversation exceeds the request limit\n");
      return -1;
    }
  if (api_key[0] != 0)
    header_length = snprintf(headers, sizeof(headers),
      "POST %s HTTP/1.1\r\nHost: %s:%d\r\n"
      "Authorization: Bearer %s\r\nContent-Type: application/json\r\n"
      "Content-Length: %zu\r\nConnection: close\r\n\r\n",
      api_path, api_host, api_port, api_key, request_length);
  else
    header_length = snprintf(headers, sizeof(headers),
      "POST %s HTTP/1.1\r\nHost: %s:%d\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: %zu\r\nConnection: close\r\n\r\n",
      api_path, api_host, api_port, request_length);
  if (header_length < 0 || (size_t) header_length >= sizeof(headers))
    return -1;
  socket_fd = connect_endpoint();
  if (socket_fd < 0)
    {
      print_formatted("error: connect %s:%d: %s\n", api_host, api_port,
                      strerror(errno));
      return -1;
    }
  if (write_all(socket_fd, headers, (size_t) header_length) < 0
      || write_all(socket_fd, request, request_length) < 0)
    {
      print_formatted("error: send request: %s\n", strerror(errno));
      close(socket_fd);
      return -1;
    }
  receive_status = receive_response(socket_fd, response_storage,
                                    MAX_RESPONSE, &response_length, &body,
                                    &receive_error);
  close(socket_fd);
  if (receive_status == -2)
    return -1;
  if (receive_status < 0)
    {
      if (interrupted)
        print_formatted("\n[request interrupted]\n");
      else
        print_formatted("error: receive response: %s\n",
                        strerror(receive_error));
      return -1;
    }
  (void) response_length;
  *response_body = body;
  return 0;
}

typedef struct {
  int result;
  const char *response_body;
  char *response_storage;
} EndpointRequest;

typedef struct {
  pthread_t worker;
  pthread_mutex_t mutex;
  pthread_cond_t completed_cond;
  int done;
  EndpointRequest request;
} EndpointWait;

static void *run_endpoint_request(void *argument)
{
  EndpointWait *wait = argument;

  wait->request.result = call_endpoint(&wait->request.response_body,
                                       wait->request.response_storage);
  pthread_mutex_lock(&wait->mutex);
  wait->done = 1;
  pthread_cond_broadcast(&wait->completed_cond);
  pthread_mutex_unlock(&wait->mutex);
  return NULL;
}

static int call_endpoint_threaded(const char **response_body,
                                  char *response_storage)
{
  EndpointWait wait;
  int create_error;
  void *thread_result;

  memset(&wait, 0, sizeof(wait));
  wait.request.response_storage = response_storage;
  pthread_mutex_init(&wait.mutex, NULL);
  pthread_cond_init(&wait.completed_cond, NULL);
  create_error = pthread_create(&wait.worker, NULL, run_endpoint_request, &wait);
  if (create_error != 0)
    {
      errno = create_error;
      print_formatted("error: create AI request thread: %s\n", strerror(errno));
      pthread_cond_destroy(&wait.completed_cond);
      pthread_mutex_destroy(&wait.mutex);
      return call_endpoint(response_body, response_storage);
    }
  begin_live_input();
  live_steering_prompt();
  for (;;)
    {
      int worker_done;

      pthread_mutex_lock(&wait.mutex);
      worker_done = wait.done;
      pthread_mutex_unlock(&wait.mutex);
      if (worker_done || interrupted)
        break;
      poll_live_input();
      sleep(1);
    }
  poll_live_input();
  end_live_input();
  if (!wait.done)
    {
      pthread_cancel(wait.worker);
      pthread_join(wait.worker, &thread_result);
      print_formatted("\n[request interrupted]\n");
      pthread_cond_destroy(&wait.completed_cond);
      pthread_mutex_destroy(&wait.mutex);
      return -1;
    }
  pthread_join(wait.worker, &thread_result);
  if (wait.request.result == 0)
    *response_body = wait.request.response_body;
  create_error = wait.request.result;
  pthread_cond_destroy(&wait.completed_cond);
  pthread_mutex_destroy(&wait.mutex);
  return create_error;
}

static int read_line(const char *prompt, char *buffer, size_t capacity)
{
  size_t length;
  int character;

  if (prompt != NULL && prompt[0] != 0)
    print_formatted("%s\n", prompt);
  if (fgets(buffer, (int) capacity, stdin) == NULL)
    return -1;
  length = strlen(buffer);
  if (length > 0 && buffer[length - 1] == '\n')
    buffer[length - 1] = 0;
  else
    {
      while ((character = getchar()) != EOF && character != '\n')
        ;
    }
  return 0;
}

static int read_multiline(const char *prompt, const char *continuation_prompt,
                          char *buffer, size_t capacity)
{
  char line[MAX_LINE];
  size_t length = 0;

  buffer[0] = 0;
  for (;;)
    {
      if (read_line(length == 0 ? prompt : continuation_prompt, line,
                    sizeof(line)) < 0)
        return -1;
      if (line[0] == 0)
        break;
      if (length + strlen(line) + 2 >= capacity)
        {
          print_formatted("error: input is too long\n");
          return -1;
        }
      if (length != 0)
        buffer[length++] = '\n';
      memcpy(buffer + length, line, strlen(line) + 1);
      length += strlen(line);
    }
  return 0;
}

static int append_tool_result(const ToolCall *call, const char *content)
{
  char json[MESSAGE_SIZE];
  Buffer buffer = {json, sizeof(json), 0};

  if (buffer_append_string(&buffer, "{\"role\":\"tool\",\"tool_call_id\":") < 0
      || append_json_text(&buffer, call->id) < 0
      || buffer_append_string(&buffer, ",\"name\":") < 0
      || append_json_text(&buffer, call->name) < 0
      || buffer_append_string(&buffer, ",\"content\":") < 0
      || append_json_text(&buffer, content) < 0
      || buffer_append_string(&buffer, "}") < 0)
    return -1;
  return add_message(json, buffer.length, 0);
}

static int append_tool_output(const ToolCall *call, const char *prefix,
                              const char *output, size_t output_length)
{
  char content[MODEL_OUTPUT_SIZE + MAX_LINE];
  char json[MESSAGE_SIZE];
  Buffer buffer = {json, sizeof(json), 0};
  size_t content_length;

  content_length = strlen(prefix);
  if (content_length >= sizeof(content)
      || output_length > sizeof(content) - content_length - 1)
    return -1;
  memcpy(content, prefix, content_length);
  memcpy(content + content_length, output, output_length);
  content[content_length + output_length] = 0;

  if (buffer_append_string(&buffer, "{\"role\":\"tool\",\"tool_call_id\":") < 0
      || append_json_text(&buffer, call->id) < 0
      || buffer_append_string(&buffer, ",\"name\":") < 0
      || append_json_text(&buffer, call->name) < 0
      || buffer_append_string(&buffer, ",\"content\":") < 0
      || append_json_string(&buffer, content, content_length + output_length) < 0
      || buffer_append_string(&buffer, "}") < 0)
    return -1;
  return add_message(json, buffer.length, 0);
}

static int add_assistant_response(const char *content, const ToolCall *calls,
                                  size_t call_count)
{
  char json[MESSAGE_SIZE];
  Buffer buffer = {json, sizeof(json), 0};
  size_t index;

  if (buffer_append_string(&buffer, "{\"role\":\"assistant\",\"content\":") < 0
      || (content[0] != 0 && append_json_text(&buffer, content) < 0)
      || (content[0] == 0 && buffer_append_string(&buffer, "null") < 0))
    return -1;
  if (call_count != 0)
    {
      if (buffer_append_string(&buffer, ",\"tool_calls\":[") < 0)
        return -1;
      for (index = 0; index < call_count; index++)
        {
          if (index != 0 && buffer_append_string(&buffer, ",") < 0)
            return -1;
          if (buffer_append_string(&buffer,
              "{\"type\":\"function\",\"id\":") < 0
              || append_json_text(&buffer, calls[index].id) < 0
              || buffer_append_string(&buffer,
                  ",\"function\":{\"name\":") < 0
              || append_json_text(&buffer, calls[index].name) < 0
              || buffer_append_string(&buffer, ",\"arguments\":") < 0
              || buffer_append_string(&buffer, "\"{") < 0
              || buffer_append_string(&buffer, "\\\"command\\\":") < 0
              || buffer_append_string(&buffer, "\\\"") < 0
              || append_json_string_body(&buffer, calls[index].command,
                                         strlen(calls[index].command)) < 0
              || buffer_append_string(&buffer, "\\\"}") < 0
              || buffer_append_string(&buffer, "\"") < 0
              || buffer_append_string(&buffer, "}}") < 0)
            return -1;
        }
      if (buffer_append_string(&buffer, "]") < 0)
        return -1;
    }
  if (buffer_append_string(&buffer, "}") < 0)
    return -1;
  return add_message(json, buffer.length, 0);
}

static int run_command(const char *command, char *output, size_t capacity,
                       size_t *output_length, int *exit_status)
{
  int pipe_fds[2];
  pid_t child;
  time_t deadline = time(NULL) + command_timeout;
  size_t captured = 0;
  int status = 0;
  int have_status = 0;
  struct pollfd descriptor;

  if (pipe2(pipe_fds, O_CLOEXEC | O_NONBLOCK) < 0)
    return -1;
  interrupted = 0;
  child = fork();
  if (child < 0)
    {
      close(pipe_fds[0]);
      close(pipe_fds[1]);
      return -1;
    }
  if (child == 0)
    {
      struct sigaction action;

      memset(&action, 0, sizeof(action));
      action.sa_handler = SIG_DFL;
      sigaction(SIGINT, &action, NULL);
      sigaction(SIGTERM, &action, NULL);
      setpgid(0, 0);
      close(pipe_fds[0]);
      dup2(pipe_fds[1], STDOUT_FILENO);
      dup2(pipe_fds[1], STDERR_FILENO);
      if (pipe_fds[1] != STDOUT_FILENO && pipe_fds[1] != STDERR_FILENO)
        close(pipe_fds[1]);
      execl("/bin/sh", "sh", "-c", command, (char *) NULL);
      _exit(127);
    }
  setpgid(child, child);
  close(pipe_fds[1]);
  descriptor.fd = pipe_fds[0];
  descriptor.events = POLLIN;
  begin_live_input();
  live_steering_prompt();
  for (;;)
    {
      char chunk[2048];
      ssize_t received;
      int poll_result;

      if (interrupted)
        kill(-child, SIGINT);
      poll_result = poll(&descriptor, 1, 100);
      if (poll_result < 0)
        {
          if (errno == EINTR)
            continue;
          break;
        }
      poll_live_input();
      if (poll_result > 0 && (descriptor.revents & (POLLIN | POLLHUP)))
        {
          received = read(pipe_fds[0], chunk, sizeof(chunk));
          if (received > 0)
            {
              if (write_all(STDOUT_FILENO, chunk, (size_t) received) < 0)
                break;
              if (captured < capacity)
                {
                  size_t amount = capacity - captured;

                  if ((size_t) received < amount)
                    amount = (size_t) received;
                  memcpy(output + captured, chunk, amount);
                  captured += amount;
                }
            }
          else if (received == 0)
            break;
          else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            break;
        }
      if (waitpid(child, &status, WNOHANG) == child)
        {
          have_status = 1;
          break;
        }
      if (time(NULL) >= deadline)
        {
          kill(-child, SIGTERM);
          sleep(1);
          kill(-child, SIGKILL);
          waitpid(child, &status, 0);
          have_status = 1;
          print_formatted("\n[command timed out after %d seconds]\n",
                          command_timeout);
          break;
        }
    }
  while (!have_status && waitpid(child, &status, 0) < 0 && errno == EINTR)
    ;
  end_live_input();
  close(pipe_fds[0]);
  if (WIFEXITED(status))
    *exit_status = WEXITSTATUS(status);
  else if (WIFSIGNALED(status))
    *exit_status = 128 + WTERMSIG(status);
  else
    *exit_status = 1;
  *output_length = captured;
  output[captured] = 0;
  return 0;
}

static int command_decision(ToolCall *call, int *run, char *steer,
                            size_t steer_capacity)
{
  char choice[MAX_LINE];

  *run = 0;
  steer[0] = 0;
  for (;;)
    {
      if (read_line("[a]llow, [e]dit, [s]teer without running, [n]o, [q]uit: ",
                    choice, sizeof(choice)) < 0)
        return -1;
      if (choice[0] == 'a' && choice[1] == 0)
        {
          *run = 1;
          return 0;
        }
      if (choice[0] == 'e' && choice[1] == 0)
        {
          if (read_line("[edited command]: ", call->command,
                        sizeof(call->command)) < 0
              || call->command[0] == 0)
            continue;
          print_formatted("[AI command edited to]: %s\n", call->command);
          *run = 1;
          return 0;
        }
      if (choice[0] == 's' && choice[1] == 0)
        {
          print_formatted("Steering ends with an empty line.\n");
          if (read_multiline("[steer]: ", "[.....]: ", steer,
                             steer_capacity) < 0
              || steer[0] == 0)
            continue;
          return 0;
        }
      if ((choice[0] == 'n' || choice[0] == 'q') && choice[1] == 0)
        return choice[0] == 'q' ? -1 : 0;
      print_formatted("Please answer a, e, s, n, or q.\n");
    }
}

static int post_execution_steer(char *steer, size_t capacity)
{
  char answer[MAX_LINE];

  for (;;)
    {
      if (steering_available())
        return 0;
      if (read_line("[s]teer before next AI request, Enter to continue, q quit: ",
                    answer, sizeof(answer)) < 0)
        return -1;
      if (answer[0] == 0)
        return 0;
      if (answer[0] == 'q' && answer[1] == 0)
        return -1;
      if (answer[0] == 's' && answer[1] == 0)
        {
          print_formatted("Steering ends with an empty line.\n");
          if (read_multiline("[steer]: ", "[.....]: ", steer, capacity) < 0
              || steer[0] == 0)
            continue;
          return 0;
        }
      print_formatted("Please answer s, Enter, or q.\n");
    }
}

static int append_bounded_output(const ToolCall *call, const char *prefix,
                                 const char *output, size_t output_length)
{
  char prefix_text[MAX_LINE];
  size_t tail = MODEL_OUTPUT_SIZE;
  size_t start;
  size_t prefix_length;

  for (;;)
    {
      start = output_length > tail ? output_length - tail : 0;
      prefix_length = (size_t) snprintf(prefix_text, sizeof(prefix_text),
                                        "%s\n", prefix);
      if (prefix_length >= sizeof(prefix_text))
        return -1;
      if (start != 0
          && prefix_length + strlen("[output tail]\n") >= sizeof(prefix_text))
        return -1;
      if (start != 0)
        strcat(prefix_text, "[output tail]\n");
      if (append_tool_output(call, prefix_text, output + start,
                             output_length - start) == 0)
        return 0;
      if (tail <= 256)
        return append_tool_result(call,
          "Command output could not be represented within the bounded context.");
      tail /= 2;
    }
}


static int process_tool_calls(ToolCall *calls, size_t call_count)
{
  static char output[CAPTURE_SIZE + 1];
  char steer[MAX_COMMAND];
  size_t index;

  steer[0] = 0;
  for (index = 0; index < call_count; index++)
    {
      int execute = 0;
      int status;
      size_t output_length = 0;
      char result[MAX_LINE];

      if (calls[index].valid && calls[index].supported)
        {
          print_formatted("\n[AI requests command]: %s\n", calls[index].command);
          if (steer[0] != 0)
            execute = 0;
          else if (full_access)
            execute = 1;
          else if (command_decision(&calls[index], &execute, steer,
                                    sizeof(steer)) < 0)
            return -1;
        }
      if (execute && calls[index].valid && calls[index].supported)
        {
          if (run_command(calls[index].command, output, CAPTURE_SIZE,
                          &output_length, &status) < 0)
            {
              snprintf(result, sizeof(result), "Execution error: %s",
                       strerror(errno));
              if (append_tool_result(&calls[index], result) < 0)
                return -1;
              continue;
            }
          snprintf(result, sizeof(result),
                   "Command completed with exit status %d. Output:", status);
          if (append_bounded_output(&calls[index], result, output,
                                    output_length) < 0)
            return -1;
          if (post_execution_steer(steer, sizeof(steer)) < 0)
            return -1;
        }
      else if (!calls[index].valid)
        {
          print_formatted("Error: malformed or missing command arguments.\n");
          if (append_tool_result(&calls[index],
              "Error: malformed or missing command arguments.") < 0)
            return -1;
        }
      else if (!calls[index].supported)
        {
          if (append_tool_result(&calls[index],
              "Error: unsupported tool; use run_terminal_command.") < 0)
            return -1;
        }
      else
        {
          snprintf(result, sizeof(result), "%s",
                   steer[0] != 0
                   ? "Execution skipped because the human supplied a new direction."
                   : "Execution refused by the human.");
          if (append_tool_result(&calls[index], result) < 0)
            return -1;
        }
    }
  if (append_live_steering(steer, sizeof(steer)) < 0)
    return -1;
  if (steer[0] != 0)
    {
      char user_message[MAX_COMMAND + 64];

      snprintf(user_message, sizeof(user_message),
               "HUMAN STEERING DURING TOOL EXECUTION: %s", steer);
      if (add_text_message("user", user_message, 0) < 0)
        return -1;
    }
  return 0;
}

static int add_pending_request_steering(void)
{
  char steering[MAX_COMMAND];

  if (!steering_available())
    return 0;
  steering[0] = 0;
  if (append_live_steering(steering, sizeof(steering)) < 0)
    return -1;
  if (steering[0] == 0)
    return 0;
  {
    char user_message[MAX_COMMAND + 64];

    snprintf(user_message, sizeof(user_message),
             "HUMAN STEERING DURING AI REQUEST: %s", steering);
    return add_text_message("user", user_message, 0);
  }
}

static int parse_tool_call(const cJSON *tool_call, ToolCall *call)
{
  const cJSON *function;
  const cJSON *id;
  const cJSON *name;
  const cJSON *arguments;
  cJSON *argument_object;
  const cJSON *command;

  memset(call, 0, sizeof(*call));
  strcpy(call->id, "tool-0");
  strcpy(call->name, "unknown");
  call->valid = 0;
  if (!cJSON_IsObject(tool_call))
    return -1;

  id = cJSON_GetObjectItemCaseSensitive(tool_call, "id");
  if (id != NULL && copy_json_string(id, call->id, sizeof(call->id)) < 0)
    return -1;
  function = cJSON_GetObjectItemCaseSensitive(tool_call, "function");
  if (!cJSON_IsObject(function))
    return -1;

  name = cJSON_GetObjectItemCaseSensitive(function, "name");
  if (copy_json_string(name, call->name, sizeof(call->name)) < 0)
    return -1;
  call->supported = strcmp(call->name, "run_terminal_command") == 0;

  arguments = cJSON_GetObjectItemCaseSensitive(function, "arguments");
  if (!cJSON_IsString(arguments))
    return -1;
  argument_object = cJSON_Parse(cJSON_GetStringValue(arguments));
  if (argument_object == NULL)
    return -1;

  command = cJSON_GetObjectItemCaseSensitive(argument_object, "command");
  if (copy_json_string(command, call->command, sizeof(call->command)) < 0)
    {
      cJSON_Delete(argument_object);
      return -1;
    }
  cJSON_Delete(argument_object);
  call->valid = call->command[0] != 0;
  return 0;
}

static int process_response(const char *body)
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

  response = cJSON_Parse(body);
  if (response == NULL)
    {
      print_formatted("error: AI response is malformed\n");
      return -1;
    }

  error = cJSON_GetObjectItemCaseSensitive(response, "error");
  if (error != NULL)
    {
      cJSON *message_value = cJSON_GetObjectItemCaseSensitive(error,
                                                             "message");

      if (copy_json_string(message_value, content, sizeof(content)) < 0)
        print_formatted("error: AI endpoint returned an error response\n");
      else
        print_formatted("error: AI endpoint: %s\n", content);
      cJSON_Delete(response);
      return -1;
    }

  choices = cJSON_GetObjectItemCaseSensitive(response, "choices");
  if (!cJSON_IsArray(choices) || cJSON_GetArraySize(choices) < 1)
    {
      print_formatted("error: AI response has no choice\n");
      cJSON_Delete(response);
      return -1;
    }

  choice = cJSON_GetArrayItem(choices, 0);
  message = cJSON_GetObjectItemCaseSensitive(choice, "message");
  if (!cJSON_IsObject(message))
    {
      print_formatted("error: AI response has no message\n");
      cJSON_Delete(response);
      return -1;
    }

  content[0] = 0;
  content_item = cJSON_GetObjectItemCaseSensitive(message, "content");
  if (content_item != NULL && !cJSON_IsNull(content_item)
      && copy_json_string(content_item, content, sizeof(content)) < 0)
    {
      print_formatted("error: assistant content is too long or malformed\n");
      cJSON_Delete(response);
      return -1;
    }

  tool_calls = cJSON_GetObjectItemCaseSensitive(message, "tool_calls");
  if (tool_calls != NULL && !cJSON_IsNull(tool_calls)
      && !cJSON_IsArray(tool_calls))
    {
      print_formatted("error: AI response is malformed\n");
      cJSON_Delete(response);
      return -1;
    }
  if (cJSON_IsArray(tool_calls))
    {
      cJSON *item;

      cJSON_ArrayForEach(item, tool_calls)
        {
          if (call_count == MAX_TOOL_CALLS)
            {
              print_formatted("error: too many tool calls in one response\n");
              cJSON_Delete(response);
              return -1;
            }
          if (parse_tool_call(item, &calls[call_count]) < 0)
            {
              strcpy(calls[call_count].id, "tool-0");
              strcpy(calls[call_count].name, "run_terminal_command");
              calls[call_count].command[0] = 0;
              calls[call_count].supported = 1;
              calls[call_count].valid = 0;
            }
          call_count++;
        }
    }

  if (add_assistant_response(content, calls, call_count) < 0)
    {
      print_formatted("error: assistant response exceeds conversation limit\n");
      cJSON_Delete(response);
      return -1;
    }
  cJSON_Delete(response);
  if (call_count == 0)
    {
      print_formatted("\n[AI Response]:\n%s\n",
                      content[0] != 0 ? content : "(empty response)");
      return 0;
    }
  if (process_tool_calls(calls, call_count) < 0)
    return -1;
  return 1;
}

static void usage(FILE *stream)
{
  fprintf(stream,
    "usage: ai [--full-access] [--ui] [--help] [--version]\n"
    "\n"
    "Environment configures AI_API_URL, AI_MODEL, AI_API_KEY or\n"
    "OPENAI_API_KEY, AI_TIMEOUT_SECONDS, and\n"
    "AI_COMMAND_TIMEOUT_SECONDS.\n");
}

static int parse_arguments(int argc, char **argv)
{
  int index;

  for (index = 1; index < argc; index++)
    {
      if (strcmp(argv[index], "--full-access") == 0)
        full_access = 1;
      else if (strcmp(argv[index], "--ui") == 0)
        ui_mode = 1;
      else if (strcmp(argv[index], "--help") == 0)
        {
          usage(stdout);
          exit(0);
        }
      else if (strcmp(argv[index], "--version") == 0)
      {
          print_formatted("ai %s %s %s\n", VERSION, BUILD_DATE, BUILD_REVISION);
          exit(0);
      }
      else
        {
          usage(stderr);
          return -1;
        }
    }
  return 0;
}

int main(int argc, char **argv)
{
  static char response[MAX_RESPONSE];
  static char prompt[MAX_COMMAND];
  const char *response_start = NULL;
  struct sigaction action;

  if (parse_arguments(argc, argv) < 0)
    return 2;
  if (configure_environment() < 0)
    return 2;
  memset(&action, 0, sizeof(action));
  action.sa_handler = on_signal;
  sigemptyset(&action.sa_mask);
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  signal(SIGPIPE, SIG_IGN);
  conversation_reset();
  print_formatted(
      "ai %s %s %s interactive AI agent\n",
      VERSION,
      BUILD_DATE,
      BUILD_REVISION
  );
  print_formatted("Endpoint: http://%s:%d%s\n", api_host, api_port, api_path);
  print_formatted("Model: %s; API timeout: %ds; command timeout: %ds\n",
                  model, api_timeout, command_timeout);
  print_formatted("Multiline input ends with an empty line. /help shows commands.\n");
  print_formatted("Live steering: type lines while AI requests or commands run.\n");
  if (full_access)
    print_formatted("Full access: commands run immediately; steering is offered after each one.\n");
  else
    print_formatted("Every command is shown for allow/edit/steer/refuse before execution.\n");
  for (;;)
    {
      if (read_multiline("\n[You]: ", "[...]: ", prompt, sizeof(prompt)) < 0)
        break;
      if (prompt[0] == 0)
        continue;
      if (strcmp(prompt, "/exit") == 0 || strcmp(prompt, "/quit") == 0)
        break;
      if (strcmp(prompt, "/new") == 0)
        {
          conversation_reset();
          clear_steering();
          print_formatted("[context reset]\n");
          continue;
        }
      if (strcmp(prompt, "/help") == 0)
        {
          print_formatted("/new reset context; /exit or /quit exit\n");
          continue;
        }
      if (add_text_message("user", prompt, 0) < 0)
        {
          print_formatted("error: prompt cannot fit in the conversation\n");
          continue;
        }
      if (call_endpoint_threaded(&response_start, response) < 0)
        {
          size_t old_count = message_count;

          drop_oldest_message();
          if (old_count == message_count)
            return 1;
          continue;
        }
      {
        int result;
        int round;
        int pending_round = 0;

        result = process_response(response_start);
        if (result < 0)
          return 1;
        pending_round = result;
        for (round = 1; result == 1; round++)
          {
            if (round == MAX_TOOL_ROUNDS)
              {
                print_formatted(
                  "\n[AI Response]: Stopped after %d tool rounds.\n",
                  MAX_TOOL_ROUNDS);
                result = 0;
                break;
              }
            if (call_endpoint_threaded(&response_start, response) < 0)
              {
                size_t old_count = message_count;

                drop_oldest_message();
                if (old_count == message_count)
                  return 1;
                round--;
                continue;
              }
            result = process_response(response_start);
            if (result < 0)
              return 1;
          }
        if (pending_round == 0 && add_pending_request_steering() < 0)
          return 1;
      }
    }
  print_formatted("\n[bye]\n");
  return 0;
}
