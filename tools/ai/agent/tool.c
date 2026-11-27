/* Built-in file tools: read_file, list_dir, and apply_patch. They take
   validated paths instead of shell lines, so workspace enforcement is
   containment (tool_confine_path) rather than lexical scanning of
   command text — variable expansion, command substitution, and encoded
   payloads never reach a shell through them. The module depends only on
   libc: results come back through ToolResult and the caller owns the
   approval boundary and the conversation. */

#define _GNU_SOURCE

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "tool.h"

/* Lines longer than this are split and numbered as two lines; files with
   such lines are better patched through the terminal tool anyway. */
#define MAX_TOOL_LINE 65536

/* Containment test shared by both resolution paths: `resolved` must be
   root itself or live under it. A root of "/" contains any absolute path. */
static int
path_under_root (const char *root, size_t root_length, const char *resolved)
{
  if (root_length == 1 && root[0] == '/')
    return 0;
  if (strncmp (resolved, root, root_length) != 0)
    return -1;
  return resolved[root_length] == 0 || resolved[root_length] == '/' ? 0 : -1;
}

/* File tools take a validated path instead of a shell line, so scope
   enforcement is containment here: the target must resolve inside the
   workspace root, symlinked or not. This bound holds regardless of
   --full-access; the terminal tool stays the wider escape hatch with its
   own approval. */
int
tool_confine_path (const char *root, const char *path, char *resolved, size_t capacity)
{
  char absolute[PATH_MAX];
  char directory[PATH_MAX];
  char *slash;
  size_t root_length = strlen (root);

  if (path[0] == '/')
    {
      if (strlen (path) >= sizeof (absolute))
        return -1;
      strcpy (absolute, path);
    }
  else if (snprintf (absolute, sizeof (absolute), "%s/%s", root, path) >= (int)sizeof (absolute))
    return -1;
  if (realpath (absolute, resolved) != NULL)
    return path_under_root (root, root_length, resolved);
  /* The leaf does not exist yet (a new file); confine its directory. */
  slash = strrchr (absolute, '/');
  if (slash == NULL)
    return -1;
  if (slash == absolute)
    strcpy (directory, "/");
  else if ((size_t)(slash - absolute) >= sizeof (directory))
    return -1;
  else
    {
      memcpy (directory, absolute, (size_t)(slash - absolute));
      directory[slash - absolute] = 0;
    }
  if (realpath (directory, resolved) == NULL)
    return -1;
  if (path_under_root (root, root_length, resolved) < 0)
    return -1;
  {
    size_t resolved_length = strlen (resolved);
    size_t remaining = capacity - resolved_length;

    if (remaining < 2 || snprintf (resolved + resolved_length, remaining, "/%s", slash + 1) >= (int)remaining)
      return -1;
  }
  return 0;
}

int
tool_read_file (const char *root, const char *path, long offset, long limit, ToolResult *result)
{
  static char output[TOOL_OUTPUT_SIZE + 1];
  char resolved[PATH_MAX];
  char line[MAX_TOOL_LINE];
  FILE *file;
  size_t captured = 0;
  long number = 0;
  long returned = 0;
  int counting = 0;

  result->body = output;
  result->body_length = 0;
  result->failed = 0;
  if (tool_confine_path (root, path, resolved, sizeof (resolved)) < 0)
    {
      snprintf (result->status, sizeof (result->status), "Error: %s resolves outside the workspace scope.", path);
      result->failed = 1;
      return 0;
    }
  file = fopen (resolved, "rb");
  if (file == NULL)
    {
      snprintf (result->status, sizeof (result->status), "Error: cannot read %s: %s", path, strerror (errno));
      result->failed = 1;
      return 0;
    }
  while (fgets (line, sizeof (line), file) != NULL)
    {
      size_t length = strlen (line);
      char numbered[MAX_TOOL_LINE + 32];
      size_t used;

      number++;
      if (number < offset || counting)
        continue;
      while (length > 0 && line[length - 1] == '\n')
        length--;
      used = (size_t)snprintf (numbered, sizeof (numbered), "%ld\t%.*s\n", number, (int)length, line);
      if (used >= sizeof (numbered))
        used = sizeof (numbered) - 1;
      if (captured + used > sizeof (output) - 1)
        used = sizeof (output) - 1 - captured;
      memcpy (output + captured, numbered, used);
      captured += used;
      returned++;
      /* Once the window is full, keep scanning so the status can report
         the true line total instead of where the capture stopped. */
      if (captured == sizeof (output) - 1 || (limit > 0 && returned == limit))
        counting = 1;
    }
  fclose (file);
  output[captured] = 0;
  result->body_length = captured;
  if (returned != 0 && (offset > 1 || limit > 0))
    snprintf (result->status, sizeof (result->status), "[file read: %s, lines %ld-%ld of %ld]", path, offset,
              offset + returned - 1, number);
  else
    snprintf (result->status, sizeof (result->status), "[file read: %s, %ld line%s]", path, returned,
              returned == 1 ? "" : "s");
  return 0;
}

int
tool_list_dir (const char *root, const char *path, ToolResult *result)
{
  static char output[TOOL_OUTPUT_SIZE + 1];
  char resolved[PATH_MAX];
  char **names = NULL;
  size_t count = 0;
  size_t captured = 0;
  size_t index;
  DIR *directory;
  struct dirent *entry;

  result->body = output;
  result->body_length = 0;
  result->failed = 0;
  if (tool_confine_path (root, path, resolved, sizeof (resolved)) < 0)
    {
      snprintf (result->status, sizeof (result->status), "Error: %s resolves outside the workspace scope.", path);
      result->failed = 1;
      return 0;
    }
  directory = opendir (resolved);
  if (directory == NULL)
    {
      snprintf (result->status, sizeof (result->status), "Error: cannot list %s: %s", path, strerror (errno));
      result->failed = 1;
      return 0;
    }
  names = malloc (TOOL_OUTPUT_SIZE * sizeof (*names));
  if (names == NULL)
    {
      closedir (directory);
      snprintf (result->status, sizeof (result->status), "Error: out of memory while listing %s.", path);
      result->failed = 1;
      return 0;
    }
  while ((entry = readdir (directory)) != NULL)
    {
      if (strcmp (entry->d_name, ".") == 0 || strcmp (entry->d_name, "..") == 0)
        continue;
      if (strlen (entry->d_name) >= TOOL_OUTPUT_SIZE || count == TOOL_OUTPUT_SIZE)
        break;
      names[count] = malloc (strlen (entry->d_name) + 1);
      if (names[count] == NULL)
        break;
      strcpy (names[count], entry->d_name);
      count++;
    }
  closedir (directory);
  /* Sort by name so two listings of the same directory agree. */
  for (index = 0; index < count; index++)
    {
      size_t swap;

      for (swap = index + 1; swap < count; swap++)
        if (strcmp (names[swap], names[index]) < 0)
          {
            char *held = names[index];

            names[index] = names[swap];
            names[swap] = held;
          }
    }
  for (index = 0; index < count; index++)
    {
      char combined[PATH_MAX];
      char formatted[TOOL_OUTPUT_SIZE + 64];
      size_t used;
      struct stat info;

      if (snprintf (combined, sizeof (combined), "%s/%s", resolved, names[index]) >= (int)sizeof (combined))
        continue;
      if (stat (combined, &info) == 0 && S_ISDIR (info.st_mode))
        used = (size_t)snprintf (formatted, sizeof (formatted), "dir  -  %s/\n", names[index]);
      else if (stat (combined, &info) == 0)
        used = (size_t)snprintf (formatted, sizeof (formatted), "file %lld %s\n", (long long)info.st_size,
                                 names[index]);
      else
        used = (size_t)snprintf (formatted, sizeof (formatted), "?    -  %s\n", names[index]);
      if (used >= sizeof (formatted))
        used = sizeof (formatted) - 1;
      if (captured + used > sizeof (output) - 1)
        used = sizeof (output) - 1 - captured;
      memcpy (output + captured, formatted, used);
      captured += used;
      if (captured == sizeof (output) - 1)
        break;
    }
  for (index = 0; index < count; index++)
    free (names[index]);
  free (names);
  output[captured] = 0;
  result->body_length = captured;
  snprintf (result->status, sizeof (result->status), "[dir listing: %s, %zu entr%s]", path, count,
            count == 1 ? "y" : "ies");
  return 0;
}

/* Whole-line SEARCH/REPLACE matching. The block must start at a line
   beginning and end at a line boundary, and must be unique, so a patch can
   never land in the wrong place silently. */
static const char *
find_line_block (const char *content, const char *needle, size_t needle_length, int *occurrences)
{
  const char *line = content;
  const char *found = NULL;

  *occurrences = 0;
  while (*line != 0)
    {
      const char *end = strchr (line, '\n');
      ptrdiff_t available = end != NULL ? end - line : (ptrdiff_t)strlen (line);

      if (available >= (ptrdiff_t)needle_length && memcmp (line, needle, needle_length) == 0
          && (line[needle_length] == '\n' || line[needle_length] == 0))
        {
          (*occurrences)++;
          found = line;
        }
      if (end == NULL)
        break;
      line = end + 1;
    }
  return found;
}

static long
count_lines (const char *text, size_t length)
{
  long lines = 0;
  size_t index;

  for (index = 0; index < length; index++)
    if (text[index] == '\n')
      lines++;
  if (length > 0 && text[length - 1] != '\n')
    lines++;
  return lines;
}

/* Diagnostic matching for a failed exact match: compare one line pair
   while ignoring blanks around the text on both sides. */
static int
line_matches_relaxed (const char *file_line, size_t file_length, const char *needle_line, size_t needle_length)
{
  size_t file_begin = 0;
  size_t file_end = file_length;
  size_t needle_begin = 0;
  size_t needle_end = needle_length;

  while (file_begin < file_end && (file_line[file_begin] == ' ' || file_line[file_begin] == '\t'))
    file_begin++;
  while (file_end > file_begin && (file_line[file_end - 1] == ' ' || file_line[file_end - 1] == '\t'))
    file_end--;
  while (needle_begin < needle_end && (needle_line[needle_begin] == ' ' || needle_line[needle_begin] == '\t'))
    needle_begin++;
  while (needle_end > needle_begin && (needle_line[needle_end - 1] == ' ' || needle_line[needle_end - 1] == '\t'))
    needle_end--;
  if (file_end - file_begin != needle_end - needle_begin)
    return 0;
  return memcmp (file_line + file_begin, needle_line + needle_begin, file_end - file_begin) == 0;
}

/* Diagnostic matching for a failed exact match: does the needle block
   line up with the file starting at `file_line`, a line beginning, once
   blanks around each line are ignored? The result only explains a miss,
   so it never has to be unique and is never applied. */
static int
block_matches_relaxed (const char *file_line, const char *content_end, const char *needle, size_t needle_length)
{
  const char *file_position = file_line;
  const char *needle_position = needle;
  const char *needle_end = needle + needle_length;

  while (needle_position < needle_end)
    {
      const char *needle_newline = memchr (needle_position, '\n', (size_t)(needle_end - needle_position));
      size_t needle_line_length = needle_newline != NULL ? (size_t)(needle_newline - needle_position)
                                                         : (size_t)(needle_end - needle_position);
      const char *file_newline = memchr (file_position, '\n', (size_t)(content_end - file_position));
      size_t file_line_length = file_newline != NULL ? (size_t)(file_newline - file_position)
                                                     : (size_t)(content_end - file_position);

      if (file_position >= content_end
          || !line_matches_relaxed (file_position, file_line_length, needle_position, needle_line_length))
        return 0;
      needle_position = needle_newline != NULL ? needle_newline + 1 : needle_end;
      file_position = file_newline != NULL ? file_newline + 1 : content_end;
    }
  return 1;
}

/* Diagnostic matching for a failed exact match: walk the file line by
   line and return the first place the needle block matches once blanks
   are ignored, with its 1-based line number for the error message. */
static const char *
find_line_block_relaxed (const char *content, const char *needle, size_t needle_length, int *line_number)
{
  const char *content_end = content + strlen (content);
  const char *line = content;
  const char *found = NULL;
  int number = 0;

  *line_number = 0;
  while (line < content_end)
    {
      const char *end = memchr (line, '\n', (size_t)(content_end - line));

      number++;
      if (found == NULL && block_matches_relaxed (line, content_end, needle, needle_length))
        {
          found = line;
          *line_number = number;
        }
      if (end == NULL)
        break;
      line = end + 1;
    }
  return found;
}

/* Render one line with its whitespace visible so the model can copy the
   exact bytes: a tab becomes \t, a carriage return \r, a backslash \\,
   other control bytes \xNN. Long lines are cut with "...". */
static void
escape_line (const char *line, size_t length, char *output, size_t capacity)
{
  static const char digits[] = "0123456789abcdef";
  size_t position = 0;
  size_t index;

  if (capacity == 0)
    return;
  /* Keep room for the "..." cut and the terminator behind the worst
     four-byte escape an iteration can emit. */
  for (index = 0; index < length && position + 8 < capacity; index++)
    {
      unsigned char byte = (unsigned char)line[index];

      if (byte == '\t' || byte == '\r' || byte == '\\')
        {
          output[position++] = '\\';
          output[position++] = byte == '\t' ? 't' : (byte == '\r' ? 'r' : '\\');
        }
      else if (byte < 32 || byte == 127)
        {
          output[position++] = '\\';
          output[position++] = 'x';
          output[position++] = digits[byte >> 4];
          output[position++] = digits[byte & 15];
        }
      else
        output[position++] = (char)byte;
    }
  if (index < length)
    {
      output[position++] = '.';
      output[position++] = '.';
      output[position++] = '.';
    }
  output[position] = 0;
}

/* Locate a line that consists of exactly the marker text. */
static const char *
find_marker_line (const char *start, const char *marker)
{
  const char *line = start;
  size_t marker_length = strlen (marker);

  while (*line != 0)
    {
      const char *end = strchr (line, '\n');
      size_t length = end != NULL ? (size_t)(end - line) : strlen (line);

      if (length == marker_length && memcmp (line, marker, length) == 0)
        return line;
      if (end == NULL)
        return NULL;
      line = end + 1;
    }
  return NULL;
}

/* Replace the block at `position` with `replacement`, keeping whole-line
   structure: the newline that ended the old block also ends the new one. */
static int
splice_block (const char *content, const char *position, size_t replaced_length, const char *replacement,
              size_t replacement_length, char *result, size_t capacity)
{
  size_t head = (size_t)(position - content);
  size_t tail_start = head + replaced_length;
  size_t tail_length;
  size_t grown = replacement_length + (replacement_length > 0 ? 1 : 0);

  if (content[tail_start] == '\n')
    tail_start++;
  tail_length = strlen (content + tail_start);
  if (head + grown + tail_length + 1 > capacity)
    return -1;
  memcpy (result, content, head);
  memcpy (result + head, replacement, replacement_length);
  if (replacement_length > 0)
    result[head + replacement_length] = '\n';
  memcpy (result + head + grown, content + tail_start, tail_length);
  result[head + grown + tail_length] = 0;
  return 0;
}

static int
load_patchable_file (const char *resolved, char *content, size_t capacity, int *exists)
{
  FILE *file;
  size_t length;

  *exists = 0;
  file = fopen (resolved, "rb");
  if (file == NULL)
    return errno == ENOENT ? 0 : -1;
  *exists = 1;
  length = fread (content, 1, capacity - 1, file);
  if (ferror (file) != 0)
    {
      fclose (file);
      errno = EIO;
      return -1;
    }
  if (fgetc (file) != EOF)
    {
      fclose (file);
      errno = EFBIG;
      return -1;
    }
  fclose (file);
  if (memchr (content, '\0', length) != NULL)
    {
      errno = EILSEQ;
      return -1;
    }
  content[length] = 0;
  return 0;
}

static int
save_patched_file (const char *resolved, const char *content)
{
  FILE *file;
  size_t length = strlen (content);

  file = fopen (resolved, "wb");
  if (file == NULL)
    return -1;
  if (fwrite (content, 1, length, file) != length || fflush (file) != 0)
    {
      fclose (file);
      errno = EIO;
      return -1;
    }
  return fclose (file) == 0 ? 0 : -1;
}

/* Copy source into destination, marking a cut with "..." when it does not
   fit; the patch summary can list more paths than the status line holds. */
static void
copy_status_text (const char *source, char *destination, size_t capacity)
{
  size_t length = strlen (source);

  if (length >= capacity)
    length = capacity > 3 ? capacity - 4 : 0;
  memcpy (destination, source, length);
  if (source[length] != 0)
    {
      memcpy (destination + length, "...", 3);
      length += 3;
    }
  destination[length] = 0;
}

/* Append one path to a comma-separated list, skipping a repeat of the
   last entry: consecutive hunks usually repeat one file. */
static void
append_path_list (char *list, size_t capacity, const char *path, size_t *length, size_t *last)
{
  if (*length != 0 && strcmp (list + *last, path) == 0)
    return;
  {
    int written = snprintf (list + *length, capacity - *length, "%s%s", *length == 0 ? "" : ", ", path);

    if (written >= 0 && (size_t)written < capacity - *length)
      {
        *last = *length + (*length == 0 ? 0 : 2);
        *length += (size_t)written;
      }
  }
}

int
tool_apply_patch (const char *root, const char *patch, ToolResult *result)
{
  static char content[MAX_PATCHABLE_FILE + 2 * MAX_PATCH + 64];
  static char scratch[MAX_PATCHABLE_FILE + 2 * MAX_PATCH + 64];
  static const char search_marker[] = "<<<<<<< SEARCH";
  static const char divide_marker[] = "=======";
  static const char replace_marker[] = ">>>>>>> REPLACE";
  const size_t buffer_capacity = sizeof (content);
  char resolved[PATH_MAX];
  char current_path[PATH_MAX];
  char loaded_path[PATH_MAX];
  char display_path[PATH_MAX];
  char touched[4 * PATH_MAX];
  char persisted[4 * PATH_MAX];
  const char *line = patch;
  size_t touched_length = 0;
  size_t touched_last = 0;
  size_t persisted_length = 0;
  size_t persisted_last = 0;
  size_t hunks = 0;
  long added = 0;
  long removed = 0;
  int loaded_exists = 0;
  int loaded = 0;
  int dirty = 0;
  int failed = 0;

  result->body = NULL;
  result->body_length = 0;
  result->failed = 0;
  current_path[0] = 0;
  loaded_path[0] = 0;
  display_path[0] = 0;
  touched[0] = 0;
  persisted[0] = 0;
  while (*line != 0)
    {
      const char *end = strchr (line, '\n');
      size_t length = end != NULL ? (size_t)(end - line) : strlen (line);

      if (length == sizeof (search_marker) - 1 && memcmp (line, search_marker, length) == 0)
        {
          const char *search_begin = end != NULL ? end + 1 : line + length;
          const char *divide = find_marker_line (search_begin, divide_marker);
          const char *replace_begin;
          const char *closing;
          const char *search;
          const char *replace;
          size_t search_length;
          size_t replace_length;

          if (divide == NULL)
            {
              snprintf (result->status, sizeof (result->status),
                        "Error: hunk %zu is missing its '=======' divider.", hunks + 1);
              failed = 1;
              break;
            }
          search = search_begin;
          search_length = (size_t)(divide - search_begin);
          if (search_length > 0)
            search_length--; /* the newline before the divider is not content */
          replace_begin = divide + sizeof (divide_marker) - 1;
          if (*replace_begin == '\r')
            replace_begin++;
          if (*replace_begin == '\n')
            replace_begin++;
          closing = find_marker_line (replace_begin, replace_marker);
          if (closing == NULL)
            {
              snprintf (result->status, sizeof (result->status),
                        "Error: hunk %zu is missing its '>>>>>>> REPLACE' marker.", hunks + 1);
              failed = 1;
              break;
            }
          replace = replace_begin;
          replace_length = (size_t)(closing - replace_begin);
          if (replace_length > 0)
            replace_length--;
          line = closing + sizeof (replace_marker) - 1;
          if (*line == '\r')
            line++;
          if (*line == '\n')
            line++;

          if (current_path[0] == 0)
            {
              snprintf (result->status, sizeof (result->status),
                        "Error: hunk %zu has no file path line before '<<<<<<< SEARCH'.", hunks + 1);
              failed = 1;
              break;
            }
          if (tool_confine_path (root, current_path, resolved, sizeof (resolved)) < 0)
            {
              snprintf (result->status, sizeof (result->status),
                        "Error: %s resolves outside the workspace scope.", current_path);
              failed = 1;
              break;
            }
          if (!loaded || strcmp (resolved, loaded_path) != 0)
            {
              if (loaded && dirty)
                {
                  if (save_patched_file (loaded_path, content) < 0)
                    {
                      snprintf (result->status, sizeof (result->status), "Error: cannot write %s: %s", loaded_path,
                                strerror (errno));
                      failed = 1;
                      break;
                    }
                  append_path_list (persisted, sizeof (persisted), display_path, &persisted_length, &persisted_last);
                }
              if (load_patchable_file (resolved, content, buffer_capacity, &loaded_exists) < 0)
                {
                  snprintf (result->status, sizeof (result->status), "Error: cannot load %s: %s", current_path,
                            strerror (errno));
                  failed = 1;
                  break;
                }
              snprintf (loaded_path, sizeof (loaded_path), "%s", resolved);
              snprintf (display_path, sizeof (display_path), "%s", current_path);
              loaded = 1;
              dirty = 0;
            }
          if (search_length == 0 && replace_length == 0)
            {
              snprintf (result->status, sizeof (result->status),
                        "Error: hunk %zu has an empty SEARCH and an empty REPLACE block.", hunks + 1);
              failed = 1;
              break;
            }
          if (search_length == 0)
            {
              if (loaded_exists)
                {
                  snprintf (result->status, sizeof (result->status),
                            "Error: %s already exists; give SEARCH lines to edit it instead.", current_path);
                  failed = 1;
                  break;
                }
              memcpy (content, replace, replace_length);
              content[replace_length] = '\n';
              content[replace_length + 1] = 0;
              loaded_exists = 1;
              dirty = 1;
            }
          else
            {
              int occurrences;
              const char *position = find_line_block (content, search, search_length, &occurrences);

              if (!loaded_exists)
                {
                  snprintf (result->status, sizeof (result->status),
                            "Error: %s does not exist; use an empty SEARCH block to create it.", current_path);
                  failed = 1;
                  break;
                }
              if (occurrences == 0)
                {
                  int near_line = 0;
                  const char *near = find_line_block_relaxed (content, search, search_length, &near_line);

                  if (near != NULL)
                    {
                      const char *content_end = content + strlen (content);
                      const char *near_end = memchr (near, '\n', (size_t)(content_end - near));
                      size_t near_length = near_end != NULL ? (size_t)(near_end - near)
                                                            : (size_t)(content_end - near);
                      char rendered[96];
                      char message[PATH_MAX + 512];

                      escape_line (near, near_length, rendered, sizeof (rendered));
                      snprintf (message, sizeof (message),
                                "Error: SEARCH text of hunk %zu not found in %s; it matches line %d except for "
                                "whitespace: the file has \"%s\" there (a tab appears as \\t). Copy the whitespace "
                                "exactly, tabs included.",
                                hunks + 1, current_path, near_line, rendered);
                      copy_status_text (message, result->status, sizeof (result->status));
                    }
                  else
                    {
                      snprintf (result->status, sizeof (result->status),
                                "Error: SEARCH text of hunk %zu not found in %s; re-read the file and copy the lines "
                                "exactly.",
                                hunks + 1, current_path);
                    }
                  failed = 1;
                  break;
                }
              if (occurrences > 1)
                {
                  snprintf (result->status, sizeof (result->status),
                            "Error: SEARCH text of hunk %zu matches %d locations in %s; add surrounding lines to "
                            "make it unique.",
                            hunks + 1, occurrences, current_path);
                  failed = 1;
                  break;
                }
              if (splice_block (content, position, search_length, replace, replace_length, scratch,
                                buffer_capacity)
                  < 0)
                {
                  snprintf (result->status, sizeof (result->status),
                            "Error: patching %s would exceed the %d byte file limit.", current_path,
                            MAX_PATCHABLE_FILE);
                  failed = 1;
                  break;
                }
              memcpy (content, scratch, strlen (scratch) + 1);
              dirty = 1;
            }
          /* Consecutive hunks usually repeat one file; list each path once. */
          append_path_list (touched, sizeof (touched), current_path, &touched_length, &touched_last);
          hunks++;
          added += count_lines (replace, replace_length);
          removed += count_lines (search, search_length);
        }
      else
        {
          /* Non-marker lines between hunks name the file that follows. */
          if (length > 0 && length < sizeof (current_path))
            {
              memcpy (current_path, line, length);
              current_path[length] = 0;
            }
          line = end != NULL ? end + 1 : line + length;
        }
    }
  if (loaded && dirty && !failed && save_patched_file (loaded_path, content) < 0)
    {
      snprintf (result->status, sizeof (result->status), "Error: cannot write %s: %s", loaded_path, strerror (errno));
      failed = 1;
      dirty = 0; /* the write error already explains itself; no rollback to report */
    }
  if (hunks == 0 && !failed)
    {
      snprintf (result->status, sizeof (result->status), "Error: the patch contains no SEARCH/REPLACE hunks.");
      result->failed = 1;
      return 0;
    }
  if (failed)
    {
      if (persisted_length != 0)
        {
          char suffix[4 * PATH_MAX + 32];

          snprintf (suffix, sizeof (suffix), " Files already patched: %s.", persisted);
          copy_status_text (suffix, result->status + strlen (result->status),
                            sizeof (result->status) - strlen (result->status));
        }
      if (loaded && dirty)
        {
          char suffix[PATH_MAX + 96];

          snprintf (suffix, sizeof (suffix), " Changes to %s were rolled back; resend all of its hunks.",
                    display_path);
          copy_status_text (suffix, result->status + strlen (result->status),
                            sizeof (result->status) - strlen (result->status));
        }
      result->failed = 1;
      return 0;
    }
  {
    char summary[4 * PATH_MAX + 192];

    snprintf (summary, sizeof (summary), "[patch applied: %zu hunk%s, +%ld -%ld lines] to %s", hunks,
              hunks == 1 ? "" : "s", added, removed, touched);
    copy_status_text (summary, result->status, sizeof (result->status));
  }
  return 0;
}

/* Pre-approval scope check: walk the patch's path lines with the same
   marker rules the applier uses and report the first path that escapes
   the workspace, so the human is never asked to approve a patch that
   would refuse midway (or worse, half-apply and then refuse). */
int
tool_patch_confined (const char *root, const char *patch, char *offender, size_t offender_capacity)
{
  static const char search_marker[] = "<<<<<<< SEARCH";
  static const char divide_marker[] = "=======";
  static const char replace_marker[] = ">>>>>>> REPLACE";
  const char *line = patch;

  while (*line != 0)
    {
      const char *end = strchr (line, '\n');
      size_t length = end != NULL ? (size_t)(end - line) : strlen (line);

      if (length == sizeof (search_marker) - 1 && memcmp (line, search_marker, length) == 0)
        {
          const char *search_begin = end != NULL ? end + 1 : line + length;
          const char *divide = find_marker_line (search_begin, divide_marker);

          if (divide == NULL)
            return 0; /* the applier reports the malformed hunk */
          {
            const char *replace_begin = divide + sizeof (divide_marker) - 1;
            const char *closing;

            if (*replace_begin == '\r')
              replace_begin++;
            if (*replace_begin == '\n')
              replace_begin++;
            closing = find_marker_line (replace_begin, replace_marker);
            if (closing == NULL)
              return 0;
            line = closing + sizeof (replace_marker) - 1;
            if (*line == '\r')
              line++;
            if (*line == '\n')
              line++;
          }
        }
      else
        {
          if (length > 0)
            {
              char resolved[PATH_MAX];

              if (length >= offender_capacity)
                length = offender_capacity - 1;
              memcpy (offender, line, length);
              offender[length] = 0;
              if (tool_confine_path (root, offender, resolved, sizeof (resolved)) < 0)
                return -1;
            }
          line = end != NULL ? end + 1 : line + length;
        }
    }
  return 0;
}
