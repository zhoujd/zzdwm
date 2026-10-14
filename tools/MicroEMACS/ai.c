/*
 * Filename ai.c
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef MINGW
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "def.h"

#if defined(LISP) && defined(AI)

#define AI_DEFAULT_MODEL       "qwen2.5-coder:1.5b"
#define AI_DEFAULT_NUM_PREDICT 128
#define AI_TIMEOUT_SECONDS     30
#define AI_MAX_REQUEST_SIZE    (1024 * 1024)
#define AI_MAX_RESPONSE_SIZE   (256 * 1024)
#define AI_ERROR_SIZE          160
#define AI_HOST_SIZE           128
#define AI_PATH_SIZE           256
#define AI_API_KEY_SIZE        256
#define AI_REASONING_EFFORT_SIZE 32
#define AI_SYSTEM_PROMPT                                                 \
  "You are a helpful assistant inside MicroEMACS. Write concise "        \
  "assistant commentary in Markdown: explain the answer, assumptions, "  \
  "and next steps. Include code when useful, but do not reply with "     \
  "code only."

#ifdef MINGW
typedef SOCKET AI_SOCKET;
typedef int AI_SOCKLEN;
#define AI_INVALID_SOCKET INVALID_SOCKET
#else
typedef int AI_SOCKET;
typedef socklen_t AI_SOCKLEN;
#define AI_INVALID_SOCKET (-1)
#endif

static char ai_error[AI_ERROR_SIZE];
static char ai_host[AI_HOST_SIZE] = "127.0.0.1";
static int ai_port = 11434;
static char ai_path[AI_PATH_SIZE] = "/api/generate";
static char ai_api_key[AI_API_KEY_SIZE];
static char ai_reasoning_effort[AI_REASONING_EFFORT_SIZE];

static int
ai_is_chat_endpoint (void)
{
  return strcmp (ai_path, "/v1/chat/completions") == 0;
}

static int
ai_valid_host (const char *host, size_t length)
{
  size_t index;

  for (index = 0; index < length; index++)
    if (isalnum ((unsigned char) host[index]) == 0
        && host[index] != '.' && host[index] != '-'
        && host[index] != '_')
      return FALSE;
  return TRUE;
}

static int
ai_valid_header_value (const char *value)
{
  size_t index;

  for (index = 0; value[index] != 0; index++)
    if (value[index] < '!' || value[index] > '~')
      return FALSE;
  return TRUE;
}

static void
ai_set_error (const char *format, ...)
{
  va_list arguments;

  va_start (arguments, format);
  vsnprintf (ai_error, sizeof (ai_error), format, arguments);
  va_end (arguments);
}

int
ai_configure (const char *endpoint, const char *api_key)
{
  const char *host;
  const char *path;
  const char *port;
  const char *end;
  size_t host_length;
  long value;

  ai_error[0] = 0;
  if (endpoint == NULL || endpoint[0] == 0)
    {
      ai_set_error ("AI endpoint is empty");
      return FALSE;
    }
  if (strncmp (endpoint, "https://", 8) == 0)
    {
      ai_set_error ("HTTPS AI endpoints are not supported");
      return FALSE;
    }
  if (strncmp (endpoint, "http://", 7) != 0)
    {
      ai_set_error ("AI endpoint must start with http://");
      return FALSE;
    }
  host = endpoint + 7;
  path = strchr (host, '/');
  end = path != NULL ? path : host + strlen (host);
  port = memchr (host, ':', (size_t) (end - host));
  if (port != NULL)
    {
      host_length = (size_t) (port - host);
      port++;
      if (port == end)
        {
          ai_set_error ("AI endpoint port is empty");
          return FALSE;
        }
      value = strtol (port, (char **) &port, 10);
      if (port != end || value <= 0 || value > 65535)
        {
          ai_set_error ("AI endpoint port is invalid");
          return FALSE;
        }
      ai_port = (int) value;
    }
  else
    {
      host_length = (size_t) (end - host);
      ai_port = 80;
    }
  if (host_length == 0 || host_length >= sizeof (ai_host)
      || ai_valid_host (host, host_length) == FALSE)
    {
      ai_set_error ("AI endpoint host is invalid");
      return FALSE;
    }
  memcpy (ai_host, host, host_length);
  ai_host[host_length] = 0;
  if (path == NULL)
    path = "/";
  if (path[0] != '/' || strlen (path) >= sizeof (ai_path)
      || ai_valid_header_value (path) == FALSE)
    {
      ai_set_error ("AI endpoint path is invalid");
      return FALSE;
    }
  strcpy (ai_path, path);
  if (api_key != NULL && strlen (api_key) >= sizeof (ai_api_key))
    {
      ai_set_error ("AI API key is too long");
      return FALSE;
    }
  if (api_key != NULL && ai_valid_header_value (api_key) == FALSE)
    {
      ai_set_error ("AI API key is invalid");
      return FALSE;
    }
  if (api_key != NULL)
    strcpy (ai_api_key, api_key);
  else
    ai_api_key[0] = 0;
  ai_reasoning_effort[0] = 0;
  return TRUE;
}

int
ai_set_reasoning_effort (const char *effort)
{
  ai_error[0] = 0;
  if (effort == NULL || effort[0] == 0)
    {
      ai_reasoning_effort[0] = 0;
      return TRUE;
    }
  if (strcmp (effort, "minimal") != 0 && strcmp (effort, "low") != 0
      && strcmp (effort, "medium") != 0 && strcmp (effort, "high") != 0)
    {
      ai_set_error ("Invalid AI reasoning effort");
      return FALSE;
    }
  strcpy (ai_reasoning_effort, effort);
  return TRUE;
}

static int
ai_wait (AI_SOCKET socket, int write)
{
  fd_set set;
  struct timeval timeout;
  int result;

  FD_ZERO (&set);
  FD_SET (socket, &set);
  timeout.tv_sec = AI_TIMEOUT_SECONDS;
  timeout.tv_usec = 0;
#ifdef MINGW
  result = select (0, write == FALSE ? &set : NULL,
                   write != FALSE ? &set : NULL, NULL, &timeout);
#else
  result = select ((int) socket + 1, write == FALSE ? &set : NULL,
                   write != FALSE ? &set : NULL, NULL, &timeout);
#endif
  return result;
}

static int
ai_retry (void)
{
#ifdef MINGW
  int error = WSAGetLastError ();

  return error == WSAEINTR || error == WSAEWOULDBLOCK
         || error == WSAEINPROGRESS;
#else
  return errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

static int
ai_connect_in_progress (void)
{
#ifdef MINGW
  return WSAGetLastError () == WSAEWOULDBLOCK;
#else
  return errno == EINPROGRESS;
#endif
}

static int
ai_winsock_startup (void)
{
#ifdef MINGW
  static int started = FALSE;
  WSADATA data;

  if (started != FALSE)
    return TRUE;
  if (WSAStartup (MAKEWORD (2, 2), &data) != 0)
    {
      ai_set_error ("Winsock initialization failed");
      return FALSE;
    }
  started = TRUE;
#endif
  return TRUE;
}

static int
ai_set_nonblocking (AI_SOCKET socket)
{
#ifdef MINGW
  u_long mode = 1;

  return ioctlsocket (socket, FIONBIO, &mode) == 0 ? TRUE : FALSE;
#else
  int flags = fcntl (socket, F_GETFL, 0);

  if (flags < 0 || fcntl (socket, F_SETFL, flags | O_NONBLOCK) < 0)
    return FALSE;
  return TRUE;
#endif
}

static int
ai_connect (AI_SOCKET socket, const struct sockaddr *address,
            AI_SOCKLEN address_length)
{
  int error = 0;
  AI_SOCKLEN length = sizeof (error);

  if (connect (socket, address, address_length) == 0)
    return TRUE;
  if (ai_connect_in_progress () == FALSE)
    {
      ai_set_error ("AI connection failed");
      return FALSE;
    }
  if (ai_wait (socket, TRUE) != 1
      || getsockopt (socket, SOL_SOCKET, SO_ERROR,
                     (char *) &error, &length) != 0 || error != 0)
    {
      ai_set_error ("AI connection timed out");
      return FALSE;
    }
  return TRUE;
}

static void
ai_close_socket (AI_SOCKET socket)
{
  if (socket == AI_INVALID_SOCKET)
    return;
#ifdef MINGW
  closesocket (socket);
#else
  close (socket);
#endif
}

static int
ai_send_all (AI_SOCKET socket, const char *data, size_t length)
{
  size_t offset = 0;

  while (offset < length)
    {
      int count;

      if (ai_wait (socket, TRUE) != 1)
        {
          ai_set_error ("AI send timed out");
          return FALSE;
        }
      count = send (socket, data + offset, (int) (length - offset), 0);
      if (count > 0)
        offset += (size_t) count;
      else if (count < 0 && ai_retry () == FALSE)
        {
          ai_set_error ("AI send failed");
          return FALSE;
        }
    }
  return TRUE;
}

static int
ai_receive_all (AI_SOCKET socket, char **response, size_t *length)
{
  size_t size = 4096;
  size_t used = 0;
  char *buffer;

  buffer = malloc (size);
  if (buffer == NULL)
    {
      ai_set_error ("AI response allocation failed");
      return FALSE;
    }
  for (;;)
    {
      int count;

      if (ai_wait (socket, FALSE) != 1)
        {
          ai_set_error ("AI receive timed out");
          free (buffer);
          return FALSE;
        }
      if (used + 1024 >= size)
        {
          char *grown;

          if (size >= AI_MAX_RESPONSE_SIZE)
            {
              ai_set_error ("AI response is too large");
              free (buffer);
              return FALSE;
            }
          size *= 2;
          grown = realloc (buffer, size);
          if (grown == NULL)
            {
              ai_set_error ("AI response allocation failed");
              free (buffer);
              return FALSE;
            }
          buffer = grown;
        }
      count = recv (socket, buffer + used,
                    (int) (size - used - 1), 0);
      if (count == 0)
        break;
      if (count < 0 && ai_retry () != FALSE)
        continue;
      if (count <= 0)
        {
          ai_set_error ("AI receive failed");
          free (buffer);
          return FALSE;
        }
      used += (size_t) count;
      if (used >= AI_MAX_RESPONSE_SIZE)
        {
          ai_set_error ("AI response is too large");
          free (buffer);
          return FALSE;
        }
    }
  buffer[used] = 0;
  *response = buffer;
  *length = used;
  return TRUE;
}

static int
ai_append (char **buffer, size_t *length, size_t *size,
           const char *data, size_t data_length)
{
  char *grown;

  if (data_length > SIZE_MAX - *length)
    return FALSE;
  if (*length + data_length >= *size)
    {
      size_t new_size = *size;

      while (*length + data_length >= new_size)
        {
          if (new_size > SIZE_MAX / 2)
            return FALSE;
          new_size *= 2;
        }
      grown = realloc (*buffer, new_size);
      if (grown == NULL)
        return FALSE;
      *buffer = grown;
      *size = new_size;
    }
  memcpy (*buffer + *length, data, data_length);
  *length += data_length;
  return TRUE;
}

static int
ai_append_json_string (char **buffer, size_t *length, size_t *size,
                       const char *text)
{
  size_t index;

  if (ai_append (buffer, length, size, "\"", 1) == FALSE)
    return FALSE;
  for (index = 0; text[index] != 0; index++)
    {
      unsigned char character = (unsigned char) text[index];
      char escape[8];

      if (character == '"' || character == '\\')
        {
          escape[0] = '\\';
          escape[1] = (char) character;
          if (ai_append (buffer, length, size, escape, 2) == FALSE)
            return FALSE;
        }
      else if (character == '\b' || character == '\f' || character == '\n'
               || character == '\r' || character == '\t')
        {
          static const char names[] = "bfnrt";
          static const char values[] = { '\b', '\f', '\n', '\r', '\t' };
          size_t value;

          for (value = 0; value < sizeof (values); value++)
            if (character == (unsigned char) values[value])
              break;
          escape[0] = '\\';
          escape[1] = names[value];
          if (ai_append (buffer, length, size, escape, 2) == FALSE)
            return FALSE;
        }
      else if (character < 0x20)
        {
          snprintf (escape, sizeof (escape), "\\u%04x", character);
          if (ai_append (buffer, length, size, escape, 6) == FALSE)
            return FALSE;
        }
      else if (ai_append (buffer, length, size,
                          text + index, 1) == FALSE)
        return FALSE;
    }
  return ai_append (buffer, length, size, "\"", 1);
}

static int
ai_build_request (const char *prompt, const char *model, int num_predict,
                  char **request, size_t *length)
{
  char header[1024];
  char tail[64];
  size_t size = 1024;
  int header_length;
  int tail_length;

  *request = malloc (size);
  if (*request == NULL)
    return FALSE;
  *length = 0;
  if (ai_is_chat_endpoint () != FALSE)
    {
      if (ai_append (request, length, &size, "{\"model\":",
                     sizeof ("{\"model\":") - 1) == FALSE
          || ai_append_json_string (request, length, &size,
                                    model != NULL ? model
                                                  : AI_DEFAULT_MODEL)
            == FALSE
          || (ai_reasoning_effort[0] != 0
              && (ai_append (request, length, &size, ",\"reasoning_effort\":",
                             sizeof (",\"reasoning_effort\":") - 1) == FALSE
                  || ai_append_json_string (request, length, &size,
                                            ai_reasoning_effort) == FALSE))
          || ai_append (request, length, &size,
                        ",\"messages\":[{\"role\":\"system\",\"content\":",
                        sizeof (",\"messages\":[{\"role\":\"system\",\"content\":")
                        - 1) == FALSE
          || ai_append_json_string (request, length, &size,
                                    AI_SYSTEM_PROMPT) == FALSE
          || ai_append (request, length, &size,
                        "},{\"role\":\"user\",\"content\":",
                        sizeof ("},{\"role\":\"user\",\"content\":") - 1)
             == FALSE
          || ai_append_json_string (request, length, &size, prompt)
            == FALSE
          || ai_append (request, length, &size,
                        "}],\"stream\":false,\"max_tokens\":",
                        sizeof ("}],\"stream\":false,\"max_tokens\":") - 1)
             == FALSE)
        return FALSE;
    }
  else
    {
      if (ai_append (request, length, &size, "{\"model\":",
                     sizeof ("{\"model\":") - 1) == FALSE
          || ai_append_json_string (request, length, &size,
                                    model != NULL ? model
                                                  : AI_DEFAULT_MODEL)
            == FALSE
          || ai_append (request, length, &size, ",\"system\":",
                         sizeof (",\"system\":") - 1) == FALSE
          || ai_append_json_string (request, length, &size,
                                    AI_SYSTEM_PROMPT) == FALSE
          || ai_append (request, length, &size, ",\"prompt\":",
                         sizeof (",\"prompt\":") - 1) == FALSE
          || ai_append_json_string (request, length, &size, prompt)
             == FALSE
          || ai_append (request, length, &size,
                        ",\"stream\":false,\"num_predict\":",
                        sizeof (",\"stream\":false,\"num_predict\":") - 1)
             == FALSE)
        return FALSE;
    }
  tail_length = snprintf (tail, sizeof (tail), "%d}", num_predict > 0
                     ? num_predict : AI_DEFAULT_NUM_PREDICT);
  if (tail_length < 0 || (size_t) tail_length >= sizeof (tail)
      || ai_append (request, length, &size, tail,
                    (size_t) tail_length) == FALSE
      || *length >= AI_MAX_REQUEST_SIZE)
    return FALSE;
  if (ai_api_key[0] != 0)
    header_length = snprintf (header, sizeof (header),
                              "POST %s HTTP/1.0\r\n"
                              "Host: %s:%d\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: %lu\r\n"
                              "Authorization: Bearer %s\r\n"
                              "Connection: close\r\n\r\n",
                              ai_path, ai_host, ai_port,
                              (unsigned long) *length, ai_api_key);
  else
    header_length = snprintf (header, sizeof (header),
                              "POST %s HTTP/1.0\r\n"
                              "Host: %s:%d\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: %lu\r\n"
                              "Connection: close\r\n\r\n",
                              ai_path, ai_host, ai_port,
                              (unsigned long) *length);
  if (header_length < 0 || (size_t) header_length >= sizeof (header))
    return FALSE;
  if (*length + (size_t) header_length >= size)
    {
      char *grown;

      do
        {
          if (size > SIZE_MAX / 2)
            return FALSE;
          size *= 2;
        }
      while (*length + (size_t) header_length >= size);
      grown = realloc (*request, size);
      if (grown == NULL)
        return FALSE;
      *request = grown;
    }
  memmove (*request + (size_t) header_length, *request, *length);
  memcpy (*request, header, (size_t) header_length);
  *length += (size_t) header_length;
  if (*length >= AI_MAX_REQUEST_SIZE)
    return FALSE;
  return TRUE;
}

static void
ai_skip_space (const char **cursor, const char *end)
{
  while (*cursor < end
         && (**cursor == ' ' || **cursor == '\t'
             || **cursor == '\r' || **cursor == '\n'))
    (*cursor)++;
}

static int
ai_hex_value (char character)
{
  if (character >= '0' && character <= '9')
    return character - '0';
  if (character >= 'a' && character <= 'f')
    return character - 'a' + 10;
  if (character >= 'A' && character <= 'F')
    return character - 'A' + 10;
  return -1;
}

static int
ai_parse_hex (const char **cursor, const char *end, unsigned long *value)
{
  unsigned long result = 0;
  int count;

  for (count = 0; count < 4; count++)
    {
      int digit;

      if (*cursor >= end)
        return FALSE;
      digit = ai_hex_value (**cursor);
      if (digit < 0)
        return FALSE;
      result = result * 16 + (unsigned long) digit;
      (*cursor)++;
    }
  *value = result;
  return TRUE;
}

static int
ai_put_utf8 (char *output, size_t *length, size_t output_size,
             unsigned long character)
{
  unsigned char encoded[4];
  int count = 0;

  if (character <= 0x7f)
    encoded[count++] = (unsigned char) character;
  else if (character <= 0x7ff)
    {
      encoded[count++] = (unsigned char) (0xc0 | (character >> 6));
      encoded[count++] = (unsigned char) (0x80 | (character & 0x3f));
    }
  else if (character <= 0xffff)
    {
      encoded[count++] = (unsigned char) (0xe0 | (character >> 12));
      encoded[count++] = (unsigned char) (0x80 | ((character >> 6) & 0x3f));
      encoded[count++] = (unsigned char) (0x80 | (character & 0x3f));
    }
  else
    {
      encoded[count++] = (unsigned char) (0xf0 | (character >> 18));
      encoded[count++] = (unsigned char) (0x80 | ((character >> 12) & 0x3f));
      encoded[count++] = (unsigned char) (0x80 | ((character >> 6) & 0x3f));
      encoded[count++] = (unsigned char) (0x80 | (character & 0x3f));
    }
  if (*length + (size_t) count >= output_size)
    return FALSE;
  memcpy (output + *length, encoded, (size_t) count);
  *length += (size_t) count;
  return TRUE;
}

static int
ai_decode_json_string (const char **cursor, const char *end,
                       char *output, size_t output_size)
{
  size_t length = 0;

  if (*cursor >= end || **cursor != '"')
    return FALSE;
  (*cursor)++;
  while (*cursor < end && **cursor != '"')
    {
      unsigned char character = (unsigned char) **cursor;

      if (character >= 0x20 && character != '\\')
        {
          if (length + 1 >= output_size)
            return FALSE;
          output[length++] = **cursor;
          (*cursor)++;
          continue;
        }
      if (character != '\\' || *cursor + 1 >= end)
        return FALSE;
      (*cursor)++;
      character = (unsigned char) **cursor;
      (*cursor)++;
      if (character == '"' || character == '\\' || character == '/')
        {
          if (length + 1 >= output_size)
            return FALSE;
          output[length++] = (char) character;
        }
      else if (character == 'b' || character == 'f' || character == 'n'
               || character == 'r' || character == 't')
        {
          static const char names[] = "bfnrt";
          static const char values[] = { '\b', '\f', '\n', '\r', '\t' };
          size_t value;

          for (value = 0; value < sizeof (values); value++)
            if (character == (unsigned char) names[value])
              break;
          if (length + 1 >= output_size)
            return FALSE;
          output[length++] = values[value];
        }
      else if (character == 'u')
        {
          unsigned long code;

          if (ai_parse_hex (cursor, end, &code) == FALSE)
            return FALSE;
          if (code >= 0xd800 && code <= 0xdbff)
            {
              unsigned long low;

              if (*cursor + 2 >= end || (*cursor)[0] != '\\'
                  || (*cursor)[1] != 'u')
                return FALSE;
              (*cursor) += 2;
              if (ai_parse_hex (cursor, end, &low) == FALSE
                  || low < 0xdc00 || low > 0xdfff)
                return FALSE;
              code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
            }
          else if (code >= 0xdc00 && code <= 0xdfff)
            return FALSE;
          if (ai_put_utf8 (output, &length, output_size, code) == FALSE)
            return FALSE;
        }
      else
        return FALSE;
    }
  if (*cursor >= end || **cursor != '"')
    return FALSE;
  (*cursor)++;
  output[length] = 0;
  return TRUE;
}

static int
ai_skip_json_string (const char **cursor, const char *end)
{
  if (*cursor >= end || **cursor != '"')
    return FALSE;
  (*cursor)++;
  while (*cursor < end && **cursor != '"')
    {
      if (**cursor == '\\')
        {
          if (*cursor + 1 >= end)
            return FALSE;
          (*cursor) += 2;
        }
      else
        (*cursor)++;
    }
  if (*cursor >= end)
    return FALSE;
  (*cursor)++;
  return TRUE;
}

static int
ai_skip_json_value (const char **cursor, const char *end)
{
  ai_skip_space (cursor, end);
  if (*cursor >= end)
    return FALSE;
  if (**cursor == '"')
    return ai_skip_json_string (cursor, end);
  if (**cursor == '{')
    {
      (*cursor)++;
      ai_skip_space (cursor, end);
      if (*cursor < end && **cursor == '}')
        {
          (*cursor)++;
          return TRUE;
        }
      for (;;)
        {
          if (*cursor >= end || **cursor != '"'
              || ai_skip_json_string (cursor, end) == FALSE)
            return FALSE;
          ai_skip_space (cursor, end);
          if (*cursor >= end || **cursor != ':')
            return FALSE;
          (*cursor)++;
          if (ai_skip_json_value (cursor, end) == FALSE)
            return FALSE;
          ai_skip_space (cursor, end);
          if (*cursor < end && **cursor == ',')
            {
              (*cursor)++;
              ai_skip_space (cursor, end);
              continue;
            }
          if (*cursor < end && **cursor == '}')
            {
              (*cursor)++;
              return TRUE;
            }
          return FALSE;
        }
    }
  if (**cursor == '[')
    {
      (*cursor)++;
      ai_skip_space (cursor, end);
      if (*cursor < end && **cursor == ']')
        {
          (*cursor)++;
          return TRUE;
        }
      for (;;)
        {
          if (ai_skip_json_value (cursor, end) == FALSE)
            return FALSE;
          ai_skip_space (cursor, end);
          if (*cursor < end && **cursor == ',')
            {
              (*cursor)++;
              ai_skip_space (cursor, end);
              continue;
            }
          if (*cursor < end && **cursor == ']')
            {
              (*cursor)++;
              return TRUE;
            }
          return FALSE;
        }
    }
  while (*cursor < end && **cursor != ',' && **cursor != '}'
         && **cursor != ']' && **cursor != ' '
         && **cursor != '\t' && **cursor != '\r' && **cursor != '\n')
    (*cursor)++;
  return *cursor != end;
}

static int
ai_extract_chat_content (const char **cursor, const char *end,
                         char *output, size_t output_size)
{
  char key[32];

  ai_skip_space (cursor, end);
  if (*cursor >= end || **cursor != '[')
    return FALSE;
  (*cursor)++;
  ai_skip_space (cursor, end);
  if (*cursor >= end || **cursor != '{')
    return FALSE;
  (*cursor)++;
  ai_skip_space (cursor, end);
  while (*cursor < end && **cursor != '}')
    {
      if (ai_decode_json_string (cursor, end, key, sizeof (key)) == FALSE
          || *cursor >= end || **cursor != ':')
        return FALSE;
      (*cursor)++;
      ai_skip_space (cursor, end);
      if (strcmp (key, "message") == 0)
        {
          if (*cursor >= end || **cursor != '{')
            return FALSE;
          (*cursor)++;
          ai_skip_space (cursor, end);
          while (*cursor < end && **cursor != '}')
            {
              if (ai_decode_json_string (cursor, end, key,
                                         sizeof (key)) == FALSE
                  || *cursor >= end || **cursor != ':')
                return FALSE;
              (*cursor)++;
              ai_skip_space (cursor, end);
              if (strcmp (key, "content") == 0)
                return ai_decode_json_string (cursor, end,
                                              output, output_size);
              if (ai_skip_json_value (cursor, end) == FALSE)
                return FALSE;
              ai_skip_space (cursor, end);
              if (*cursor < end && **cursor == ',')
                {
                  (*cursor)++;
                  ai_skip_space (cursor, end);
                }
              else if (*cursor >= end || **cursor != '}')
                return FALSE;
            }
          return FALSE;
        }
      if (ai_skip_json_value (cursor, end) == FALSE)
        return FALSE;
      ai_skip_space (cursor, end);
      if (*cursor < end && **cursor == ',')
        {
          (*cursor)++;
          ai_skip_space (cursor, end);
        }
      else if (*cursor >= end || **cursor != '}')
        return FALSE;
    }
  return FALSE;
}

static int
ai_extract_response (const char *body, size_t length,
                     char *output, size_t output_size)
{
  const char *cursor = body;
  const char *end = body + length;

  ai_skip_space (&cursor, end);
  if (cursor >= end || *cursor != '{')
    return FALSE;
  cursor++;
  ai_skip_space (&cursor, end);
  while (cursor < end && *cursor != '}')
    {
      char key[32];

      if (ai_decode_json_string (&cursor, end, key, sizeof (key)) == FALSE
          || cursor >= end || *cursor != ':')
        return FALSE;
      cursor++;
      ai_skip_space (&cursor, end);
      if (strcmp (key, "response") == 0)
        return ai_decode_json_string (&cursor, end, output, output_size);
      if (strcmp (key, "choices") == 0
          && ai_extract_chat_content (&cursor, end,
                                      output, output_size) != FALSE)
        return TRUE;
      if (ai_skip_json_value (&cursor, end) == FALSE)
        return FALSE;
      ai_skip_space (&cursor, end);
      if (cursor < end && *cursor == ',')
        {
          cursor++;
          ai_skip_space (&cursor, end);
        }
      else if (cursor >= end || *cursor != '}')
        return FALSE;
    }
  return FALSE;
}

static const char *
ai_find_header_end (const char *response, size_t length, size_t *header_length)
{
  size_t index;

  for (index = 0; index + 3 < length; index++)
    if (memcmp (response + index, "\r\n\r\n", 4) == 0)
      {
        *header_length = index + 4;
        return response + *header_length;
      }
  for (index = 0; index + 1 < length; index++)
    if (memcmp (response + index, "\n\n", 2) == 0)
      {
        *header_length = index + 2;
        return response + *header_length;
      }
  return NULL;
}

static int
ai_header_is_chunked (const char *response, size_t header_length)
{
  size_t index;

  for (index = 0; index < header_length;)
    {
      size_t end = index;
      size_t length;

      while (end < header_length
             && response[end] != '\r' && response[end] != '\n')
        end++;
      length = end - index;
      if (length >= 18
          && strncasecmp (response + index, "Transfer-Encoding:", 18) == 0)
        {
          size_t offset;

          for (offset = 18; offset < length; offset++)
            if (strncasecmp (response + index + offset, "chunked", 7) == 0)
              return TRUE;
        }
      index = end;
      while (index < header_length
             && (response[index] == '\r' || response[index] == '\n'))
        index++;
    }
  return FALSE;
}

static int
ai_decode_chunked (const char *body, size_t length,
                   char **decoded, size_t *decoded_length)
{
  size_t offset = 0;
  size_t used = 0;
  char *buffer;

  buffer = malloc (length + 1);
  if (buffer == NULL)
    return FALSE;
  while (offset < length)
    {
      char *end;
      unsigned long chunk;

      while (offset < length
             && (body[offset] == '\r' || body[offset] == '\n'))
        offset++;
      if (offset >= length)
        break;
      chunk = strtoul (body + offset, &end, 16);
      if (end == body + offset || chunk > AI_MAX_RESPONSE_SIZE)
        {
          free (buffer);
          return FALSE;
        }
      offset = (size_t) (end - body);
      if (offset < length && body[offset] == ';')
        {
          offset++;
          while (offset < length
                 && body[offset] != '\r' && body[offset] != '\n')
            offset++;
        }
      while (offset < length
             && (body[offset] == '\r' || body[offset] == '\n'
                 || body[offset] == ';'))
        offset++;
      if (chunk > length - offset)
        {
          free (buffer);
          return FALSE;
        }
      if (chunk == 0)
        break;
      memcpy (buffer + used, body + offset, chunk);
      used += chunk;
      offset += chunk;
    }
  buffer[used] = 0;
  *decoded = buffer;
  *decoded_length = used;
  return TRUE;
}

static int
ai_response_has_content (const char *response)
{
  size_t index;

  for (index = 0; response[index] != 0; index++)
    if (isspace ((unsigned char) response[index]) == 0)
      return TRUE;
  return FALSE;
}

static int
ai_parse_response (const char *response, size_t length,
                   char *output, size_t output_size)
{
  const char *body;
  char *decoded = NULL;
  size_t header_length;
  size_t body_length;
  char *space;
  long status;
  int result;

  body = ai_find_header_end (response, length, &header_length);
  if (body == NULL || header_length < 12
      || memcmp (response, "HTTP/", 5) != 0)
    {
      ai_set_error ("Invalid AI HTTP response");
      return FALSE;
    }
  space = memchr (response, ' ', header_length);
  if (space == NULL)
    {
      ai_set_error ("Invalid AI HTTP status");
      return FALSE;
    }
  status = strtol (space + 1, &space, 10);
  if (status != 200)
    {
      ai_set_error ("AI HTTP status %ld", status);
      return FALSE;
    }
  if (ai_header_is_chunked (response, header_length) != FALSE)
    {
      body_length = length - header_length;
      if (ai_decode_chunked (body, body_length,
                             &decoded, &body_length) == FALSE)
        {
          ai_set_error ("Invalid AI chunked response");
          return FALSE;
        }
      body = decoded;
    }
  else
    body_length = length - header_length;
  result = ai_extract_response (body, body_length,
                                output, output_size);
  if (result == FALSE)
    ai_set_error ("AI response field is missing");
  else if (ai_response_has_content (output) == FALSE)
    {
      ai_set_error ("AI response content is empty");
      result = FALSE;
    }
  free (decoded);
  return result;
}

const char *
ai_last_error (void)
{
  return ai_error;
}

int
ai_complete (const char *prompt, const char *model, int num_predict,
             char *output, size_t output_size)
{
  struct addrinfo hints;
  struct addrinfo *addresses = NULL;
  struct addrinfo *address;
  char port_string[16];
  AI_SOCKET ai_socket = AI_INVALID_SOCKET;
  char *request = NULL;
  char *response = NULL;
  size_t request_length = 0;
  size_t response_length = 0;
  int result = FALSE;

  ai_error[0] = 0;
  if (prompt == NULL || prompt[0] == 0
      || (model != NULL && model[0] == 0) || output == NULL
      || output_size == 0 || num_predict < 0 || num_predict > INT_MAX)
    {
      ai_set_error ("Invalid AI request");
      return FALSE;
    }
  if (ai_winsock_startup () == FALSE
      || ai_build_request (prompt, model, num_predict,
                           &request, &request_length) == FALSE)
    {
      ai_set_error ("AI request allocation failed");
      goto done;
    }
  snprintf (port_string, sizeof (port_string), "%d", ai_port);
  memset (&hints, 0, sizeof (hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo (ai_host, port_string, &hints, &addresses) != 0)
    {
      ai_set_error ("AI host lookup failed");
      goto done;
    }
  for (address = addresses; address != NULL; address = address->ai_next)
    {
      ai_socket = socket (address->ai_family, address->ai_socktype,
                          address->ai_protocol);
      if (ai_socket == AI_INVALID_SOCKET)
        continue;
      if (ai_set_nonblocking (ai_socket) == FALSE
          || ai_connect (ai_socket, address->ai_addr,
                         (AI_SOCKLEN) address->ai_addrlen) == FALSE)
        {
          ai_close_socket (ai_socket);
          ai_socket = AI_INVALID_SOCKET;
          continue;
        }
      break;
    }
  if (ai_socket == AI_INVALID_SOCKET)
    {
      if (ai_error[0] == 0)
        ai_set_error ("AI connection failed");
      goto done;
    }
  if (ai_send_all (ai_socket, request, request_length) == FALSE
      || ai_receive_all (ai_socket, &response, &response_length) == FALSE)
    {
      if (ai_error[0] == 0)
        ai_set_error ("AI request failed");
      goto done;
    }
  if (ai_parse_response (response, response_length,
                         output, output_size) == TRUE)
    result = TRUE;

done:
  ai_close_socket (ai_socket);
  freeaddrinfo (addresses);
  free (request);
  free (response);
  return result;
}

#endif
