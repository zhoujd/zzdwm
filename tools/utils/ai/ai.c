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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "version.h"

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
  const char *cursor;
  const char *end;
} JsonParser;

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

static void on_signal(int signal_number)
{
  (void) signal_number;
  interrupted = 1;
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

static int append_json_unicode(Buffer *buffer, unsigned int codepoint)
{
  char utf8[4];
  size_t length;

  if (codepoint == 0)
    return -1;
  if (codepoint < 0x80)
    {
      utf8[0] = (char) codepoint;
      length = 1;
    }
  else if (codepoint < 0x800)
    {
      utf8[0] = (char) (0xc0 | (codepoint >> 6));
      utf8[1] = (char) (0x80 | (codepoint & 0x3f));
      length = 2;
    }
  else if (codepoint < 0x10000)
    {
      utf8[0] = (char) (0xe0 | (codepoint >> 12));
      utf8[1] = (char) (0x80 | ((codepoint >> 6) & 0x3f));
      utf8[2] = (char) (0x80 | (codepoint & 0x3f));
      length = 3;
    }
  else
    {
      utf8[0] = (char) (0xf0 | (codepoint >> 18));
      utf8[1] = (char) (0x80 | ((codepoint >> 12) & 0x3f));
      utf8[2] = (char) (0x80 | ((codepoint >> 6) & 0x3f));
      utf8[3] = (char) (0x80 | (codepoint & 0x3f));
      length = 4;
    }
  return buffer_append(buffer, utf8, length);
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

static void json_skip_whitespace(JsonParser *parser)
{
  while (parser->cursor < parser->end
         && (*parser->cursor == ' ' || *parser->cursor == '\t'
             || *parser->cursor == '\r' || *parser->cursor == '\n'))
    parser->cursor++;
}

static int hex_digit(char character)
{
  if (character >= '0' && character <= '9')
    return character - '0';
  if (character >= 'a' && character <= 'f')
    return character - 'a' + 10;
  if (character >= 'A' && character <= 'F')
    return character - 'A' + 10;
  return -1;
}

static int parse_json_escape(JsonParser *parser, Buffer *output)
{
  unsigned int codepoint;
  int index;
  int digit;

  if (parser->cursor == parser->end || *parser->cursor != 'u')
    {
      static const char valid[] = "\"\\/bfnrt";
      char character = parser->cursor != parser->end ? *parser->cursor : 0;

      if (character == 0 || strchr(valid, character) == NULL)
        return -1;
      switch (character)
        {
        case 'b': character = '\b'; break;
        case 'f': character = '\f'; break;
        case 'n': character = '\n'; break;
        case 'r': character = '\r'; break;
        case 't': character = '\t'; break;
        default: break;
        }
      if (buffer_append(output, &character, 1) < 0)
        return -1;
      parser->cursor++;
      return 0;
    }
  parser->cursor++;
  codepoint = 0;
  for (index = 0; index < 4; index++)
    {
      if (parser->cursor == parser->end)
        return -1;
      digit = hex_digit(*parser->cursor++);
      if (digit < 0)
        return -1;
      codepoint = (codepoint << 4) | (unsigned int) digit;
    }
  if (codepoint >= 0xd800 && codepoint <= 0xdbff)
    {
      unsigned int low = 0;

      if (parser->end - parser->cursor < 6 || parser->cursor[0] != '\\'
          || parser->cursor[1] != 'u')
        return -1;
      parser->cursor += 2;
      for (index = 0; index < 4; index++)
        {
          if (parser->cursor == parser->end)
            return -1;
          digit = hex_digit(*parser->cursor++);
          if (digit < 0)
            return -1;
          low = (low << 4) | (unsigned int) digit;
        }
      if (low < 0xdc00 || low > 0xdfff)
        return -1;
      codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
    }
  else if (codepoint >= 0xdc00 && codepoint <= 0xdfff)
    return -1;
  return append_json_unicode(output, codepoint);
}

static int parse_json_string(JsonParser *parser, Buffer *output)
{
  if (parser->cursor == parser->end || *parser->cursor != '"')
    return -1;
  parser->cursor++;
  while (parser->cursor < parser->end)
    {
      char character = *parser->cursor++;

      if (character == '"')
        return 0;
      if (character == 0 || (unsigned char) character < 0x20)
        return -1;
      if (character == '\\')
        {
          if (parse_json_escape(parser, output) < 0)
            return -1;
        }
      else if (buffer_append(output, &character, 1) < 0)
        return -1;
    }
  return -1;
}

static int skip_json_string(JsonParser *parser)
{
  if (parser->cursor == parser->end || *parser->cursor != '"')
    return -1;
  parser->cursor++;
  while (parser->cursor < parser->end)
    {
      char character = *parser->cursor++;

      if (character == '"')
        return 0;
      if (character == 0 || (unsigned char) character < 0x20)
        return -1;
      if (character == '\\')
        {
          if (parser->cursor == parser->end)
            return -1;
          parser->cursor++;
        }
    }
  return -1;
}

static int skip_json_value(JsonParser *parser, int depth)
{
  if (depth > 32 || parser->cursor == parser->end)
    return -1;
  json_skip_whitespace(parser);
  switch (*parser->cursor)
    {
    case '{':
    case '[':
      {
        char open = *parser->cursor++;
        char close = open == '{' ? '}' : ']';

        json_skip_whitespace(parser);
        if (parser->cursor != parser->end && *parser->cursor == close)
          {
            parser->cursor++;
            return 0;
          }
        while (parser->cursor < parser->end)
          {
            json_skip_whitespace(parser);
            if (open == '{')
              {
                if (skip_json_string(parser) < 0)
                  return -1;
                json_skip_whitespace(parser);
                if (parser->cursor == parser->end || *parser->cursor != ':')
                  return -1;
                parser->cursor++;
              }
            if (skip_json_value(parser, depth + 1) < 0)
              return -1;
            json_skip_whitespace(parser);
            if (parser->cursor != parser->end && *parser->cursor == close)
              {
                parser->cursor++;
                return 0;
              }
            if (parser->cursor == parser->end || *parser->cursor != ',')
              return -1;
            parser->cursor++;
          }
        return -1;
      }
    case '"':
      return skip_json_string(parser);
    case 't':
      if (parser->end - parser->cursor < 4
          || memcmp(parser->cursor, "true", 4) != 0)
        return -1;
      parser->cursor += 4;
      return 0;
    case 'f':
      if (parser->end - parser->cursor < 5
          || memcmp(parser->cursor, "false", 5) != 0)
        return -1;
      parser->cursor += 5;
      return 0;
    case 'n':
      if (parser->end - parser->cursor < 4
          || memcmp(parser->cursor, "null", 4) != 0)
        return -1;
      parser->cursor += 4;
      return 0;
    default:
      {
        const char *start = parser->cursor;
        int digits = 0;

        if (parser->cursor != parser->end && *parser->cursor == '-')
          parser->cursor++;
        while (parser->cursor < parser->end
               && ((*parser->cursor >= '0' && *parser->cursor <= '9')
                   || *parser->cursor == '.' || *parser->cursor == 'e'
                   || *parser->cursor == 'E' || *parser->cursor == '+'
                   || *parser->cursor == '-'))
          {
            if (*parser->cursor >= '0' && *parser->cursor <= '9')
              digits++;
            parser->cursor++;
          }
        if (digits == 0 || parser->cursor == start)
          return -1;
        return 0;
      }
    }
  return -1;
}

static const char *json_member(const char *object, const char *end,
                               const char *wanted)
{
  JsonParser parser = {object, end};
  char key[256];
  Buffer output = {key, sizeof(key) - 1, 0};

  json_skip_whitespace(&parser);
  if (parser.cursor == parser.end || *parser.cursor != '{')
    return NULL;
  parser.cursor++;
      json_skip_whitespace(&parser);
      if (parser.cursor != parser.end && *parser.cursor == '}')
        return parser.cursor - 1;
  while (parser.cursor < parser.end)
    {
      const char *value;

      output.length = 0;
      if (parse_json_string(&parser, &output) < 0)
        return NULL;
      key[output.length] = 0;
      json_skip_whitespace(&parser);
      if (parser.cursor == parser.end || *parser.cursor != ':')
        return NULL;
      parser.cursor++;
      json_skip_whitespace(&parser);
      value = parser.cursor;
      if (strcmp(key, wanted) == 0)
        return value;
      parser.cursor = value;
      if (skip_json_value(&parser, 1) < 0)
        return NULL;
      json_skip_whitespace(&parser);
      if (parser.cursor != parser.end && *parser.cursor == '}')
        return NULL;
      if (parser.cursor == parser.end || *parser.cursor != ',')
        return NULL;
      parser.cursor++;
    }
  return NULL;
}

static int json_string_value(const char *value, const char *end,
                             char *output, size_t capacity)
{
  JsonParser parser = {value, end};
  Buffer buffer = {output, capacity - 1, 0};

  if (capacity == 0 || parse_json_string(&parser, &buffer) < 0)
    return -1;
  output[buffer.length] = 0;
  return 0;
}

static int json_array_first(const char *array, const char *end,
                            const char **element)
{
  JsonParser parser = {array, end};

  json_skip_whitespace(&parser);
  if (parser.cursor == parser.end || *parser.cursor != '[')
    return -1;
  parser.cursor++;
  json_skip_whitespace(&parser);
  if (parser.cursor != parser.end && *parser.cursor == ']')
    return -1;
  *element = parser.cursor;
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

static int call_endpoint(const char **response_start, const char **response_end,
                         char *response_storage)
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
  *response_start = body;
  *response_end = body + response_length;
  return 0;
}

static int read_line(const char *prompt, char *buffer, size_t capacity)
{
  size_t length;
  int character;

  if (prompt != NULL && prompt[0] != 0)
    print_formatted("%s", prompt);
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

static int read_multiline(const char *prompt, char *buffer, size_t capacity)
{
  char line[MAX_LINE];
  size_t length = 0;

  buffer[0] = 0;
  for (;;)
    {
      if (read_line(length == 0 ? prompt : "[...]: ", line, sizeof(line)) < 0)
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
          if (read_line("[steer]: ", steer, steer_capacity) < 0
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
      if (read_line("[s]teer before next AI request, Enter to continue, q quit: ",
                    answer, sizeof(answer)) < 0)
        return -1;
      if (answer[0] == 0)
        return 0;
      if (answer[0] == 'q' && answer[1] == 0)
        return -1;
      if (answer[0] == 's' && answer[1] == 0)
        {
          if (read_line("[steer]: ", steer, capacity) < 0 || steer[0] == 0)
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

static int parse_tool_call(const char *object, const char *end, ToolCall *call)
{
  const char *value;
  const char *function;
  const char *arguments;

  memset(call, 0, sizeof(*call));
  strcpy(call->id, "tool-0");
  strcpy(call->name, "unknown");
  call->valid = 0;
  value = json_member(object, end, "id");
  if (value != NULL && json_string_value(value, end, call->id,
                                         sizeof(call->id)) < 0)
    return -1;
  function = json_member(object, end, "function");
  if (function == NULL)
    return -1;
  value = json_member(function, end, "name");
  if (value == NULL || json_string_value(value, end, call->name,
                                         sizeof(call->name)) < 0)
    return -1;
  call->supported = strcmp(call->name, "run_terminal_command") == 0;
  arguments = json_member(function, end, "arguments");
  if (arguments == NULL)
    return -1;
  {
    char arguments_json[MAX_COMMAND];
    size_t arguments_length;
    const char *command;
    const char *arguments_end;

    if (json_string_value(arguments, end, arguments_json,
                          sizeof(arguments_json)) < 0)
      return -1;
    arguments_length = strlen(arguments_json);
    arguments_end = arguments_json + arguments_length;
    command = json_member(arguments_json,
                          arguments_end, "command");
    if (command == NULL
        || json_string_value(command, arguments_end, call->command,
                             sizeof(call->command)) < 0)
      return -1;
  }
  call->valid = call->command[0] != 0;
  return 0;
}

static int process_response(const char *start, const char *end)
{
  const char *choices;
  const char *choice;
  const char *message;
  const char *content_value;
  const char *tool_calls;
  char content[MAX_LINE];
  ToolCall calls[MAX_TOOL_CALLS];
  size_t call_count = 0;
  JsonParser parser;
  const char *error;

  error = json_member(start, end, "error");
  if (error != NULL)
    {
      const char *message_value = json_member(error, end, "message");

      if (message_value == NULL
          || json_string_value(message_value, end, content,
                               sizeof(content)) < 0)
        print_formatted("error: AI endpoint returned an error response\n");
      else
        print_formatted("error: AI endpoint: %s\n", content);
      return -1;
    }
  choices = json_member(start, end, "choices");
  if (choices == NULL || json_array_first(choices, end, &choice) < 0)
    {
      print_formatted("error: AI response has no choice\n");
      return -1;
    }
  message = json_member(choice, end, "message");
  if (message == NULL)
    {
      print_formatted("error: AI response has no message\n");
      return -1;
    }
  content[0] = 0;
  content_value = json_member(message, end, "content");
  if (content_value != NULL && *content_value == '"'
      && json_string_value(content_value, end, content,
                           sizeof(content)) < 0)
    {
      print_formatted("error: assistant content is too long or malformed\n");
      return -1;
    }
  tool_calls = json_member(message, end, "tool_calls");
  if (tool_calls != NULL)
    {
      parser.cursor = tool_calls;
      parser.end = end;
      json_skip_whitespace(&parser);
      if (parser.cursor != parser.end && *parser.cursor == '[')
        {
          parser.cursor++;
          json_skip_whitespace(&parser);
          while (parser.cursor < parser.end)
            {
              if (*parser.cursor == ']')
                break;
              if (call_count == MAX_TOOL_CALLS)
                {
                  print_formatted("error: too many tool calls in one response\n");
                  return -1;
                }
              if (parse_tool_call(parser.cursor, end, &calls[call_count]) < 0)
                {
                  strcpy(calls[call_count].id, "tool-0");
                  strcpy(calls[call_count].name, "run_terminal_command");
                  calls[call_count].command[0] = 0;
                  calls[call_count].supported = 1;
                  calls[call_count].valid = 0;
                }
              call_count++;
              if (skip_json_value(&parser, 1) < 0)
                {
                  print_formatted("error: malformed tool_calls array\n");
                  return -1;
                }
              json_skip_whitespace(&parser);
              if (parser.cursor != parser.end && *parser.cursor == ',')
                {
                  parser.cursor++;
                  json_skip_whitespace(&parser);
                }
              else if (parser.cursor == parser.end || *parser.cursor != ']')
                return -1;
            }
        }
      if (parser.cursor != parser.end && *parser.cursor != ']')
        return -1;
    }
  if (add_assistant_response(content, calls, call_count) < 0)
    {
      print_formatted("error: assistant response exceeds conversation limit\n");
      return -1;
    }
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
    "usage: ai [--full-access] [--help] [--version]\n"
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
  const char *response_end = NULL;
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
  if (full_access)
    print_formatted("Full access: commands run immediately; steering is offered after each one.\n");
  else
    print_formatted("Every command is shown for allow/edit/steer/refuse before execution.\n");
  for (;;)
    {
      if (read_multiline("\n[You]: ", prompt, sizeof(prompt)) < 0)
        break;
      if (prompt[0] == 0)
        continue;
      if (strcmp(prompt, "/exit") == 0 || strcmp(prompt, "/quit") == 0)
        break;
      if (strcmp(prompt, "/new") == 0)
        {
          conversation_reset();
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
      if (call_endpoint(&response_start, &response_end, response) < 0)
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

        result = process_response(response_start, response_end);
        if (result < 0)
          return 1;
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
            if (call_endpoint(&response_start, &response_end, response) < 0)
              {
                size_t old_count = message_count;

                drop_oldest_message();
                if (old_count == message_count)
                  return 1;
                round--;
                continue;
              }
            result = process_response(response_start, response_end);
            if (result < 0)
              return 1;
          }
      }
    }
  print_formatted("\n[bye]\n");
  return 0;
}
