/*
 * Filename lisp.c
 */

#include "def.h"
#include "tinylisp.h"
#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifdef LISP

#define LISP_EXPRESSION_SIZE  256
#define LISP_OUTPUT_SIZE      256
#define LISP_COMMAND_OVERHEAD 13
#define LISP_CONFIG_FILE      ".mel"

#define LISP_DIRED_ENTRY_COLUMN 42

static int lisp_message_displayed;
static int prompt_aborted;
static int lisp_debug_level;
static char **lisp_loaded_modules;
static size_t lisp_loaded_module_count;
static size_t lisp_loaded_module_capacity;

static int lisp_load_file (int argc, const char *const *argv,
                           char *result, size_t result_size);
static int lisp_require (int argc, const char *const *argv,
                         char *result, size_t result_size);
static int lispload (const char *filename, int report);

static void show_prompt_abort (void);
static int prompt_abort_column (void);
static int
lisp_key_code (const char *name)
{
  int key = 0;

  while (*name != 0)
    {
      if (strncasecmp (name, "C-X ", 4) == 0)
        {
          key |= KCTLX;
          name += 4;
          continue;
        }
      if (strncasecmp (name, "M-", 2) == 0)
        {
          key |= KMETA;
          name += 2;
          continue;
        }
      if (strncasecmp (name, "C-", 2) == 0)
        {
          key |= KCTRL;
          name += 2;
          continue;
        }
      break;
    }
  if (name[0] == 0 || name[1] != 0)
    return -1;
  if (name[0] >= 'a' && name[0] <= 'z')
    key |= name[0] - 'a' + 'A';
  else
    key |= name[0];
  return key;
}

static int
lisp_bind_key (int argc, const char *const *argv,
               char *result, size_t result_size)
{
  SYMBOL *symbol;
  int key;

  (void) result;
  (void) result_size;
  if (argc != 2)
    return FALSE;
  key = lisp_key_code (argv[0]);
  if (key < 0 || (symbol = symlookup (argv[1])) == NULL)
    return FALSE;
  setbinding (key, symbol);
  return TRUE;
}

static int
lisp_bind_mode_key (int argc, const char *const *argv,
                    char *result, size_t result_size)
{
  SYMBOL *symbol;
  int key;

  (void) result;
  (void) result_size;
  if (argc != 2 || curbp == NULL || curbp->b_mode == NULL)
    return FALSE;
  key = lisp_key_code (argv[0]);
  if (key < 0 || (symbol = symlookup (argv[1])) == NULL)
    return FALSE;
  setmodebinding (key, symbol);
  return TRUE;
}

static int
lisp_integer (const char *text, int *value)
{
  char *end;
  long parsed;

  if (text == NULL || text[0] == 0)
    return FALSE;
  parsed = strtol (text, &end, 10);
  if (*end != 0 || parsed < INT_MIN || parsed > INT_MAX)
    return FALSE;
  *value = (int) parsed;
  return TRUE;
}

static int
lisp_execute_command (int argc, const char *const *argv,
                      char *result, size_t result_size)
{
  SYMBOL *symbol;
  int count = 1;
  int explicit_count;

  (void) result;
  (void) result_size;
  if (argc < 1 || argc > 2 || curwp == NULL)
    return FALSE;
  explicit_count = argc == 2;
  if (explicit_count && lisp_integer (argv[1], &count) == FALSE)
    return FALSE;
  if ((symbol = symlookup (argv[0])) == NULL)
    return FALSE;
  if (symbol->s_macro != NULL && (kbdmip != NULL || kbdmop != NULL))
    return FALSE;

  startsaveundo ();
  if (symbol->s_macro != NULL)
    count = domacro (symbol->s_macro, count);
  else if (symbol->s_funcp != NULL)
    count = (*symbol->s_funcp) (explicit_count, count, KRANDOM);
  else
    count = FALSE;
  endsaveundo ();
  return count;
}

static int
lisp_insert_once (const char *text)
{
  const char *start = text;

  for (;;)
    {
      const char *newline = strchr (start, '\n');
      size_t length = newline != NULL
                      ? (size_t) (newline - start) : strlen (start);

      if (length != 0
          && linsert ((int) length, 0, (char *) start) == FALSE)
        return FALSE;
      if (newline == NULL)
        return TRUE;
      if (lnewline () == FALSE)
        return FALSE;
      start = newline + 1;
    }
}

static int
lisp_insert_string (int argc, const char *const *argv,
                    char *result, size_t result_size)
{
  int count = 1;
  int index;

  (void) result;
  (void) result_size;
  if (argc < 1 || argc > 2 || curwp == NULL)
    return FALSE;
  if (argc == 2
      && (lisp_integer (argv[1], &count) == FALSE || count <= 0))
    return FALSE;
  startsaveundo ();
  for (index = 0; index < count; index++)
    {
      if (lisp_insert_once (argv[0]) == FALSE)
        {
          endsaveundo ();
          return FALSE;
        }
    }
  endsaveundo ();
  return TRUE;
}

static int
lisp_goto_bol (int argc, const char *const *argv,
               char *result, size_t result_size)
{
  (void) argv;
  (void) result;
  (void) result_size;
  if (argc != 0 || curwp == NULL)
    return FALSE;
  return gotobol (FALSE, 1, KRANDOM);
}

static int
lisp_goto_eol (int argc, const char *const *argv,
               char *result, size_t result_size)
{
  (void) argv;
  (void) result;
  (void) result_size;
  if (argc != 0 || curwp == NULL)
    return FALSE;
  return gotoeol (FALSE, 1, KRANDOM);
}

static int
lisp_forward_line (int argc, const char *const *argv,
                   char *result, size_t result_size)
{
  int count = 1;
  int status;

  (void) result;
  (void) result_size;
  if (argc > 1 || curwp == NULL)
    return FALSE;
  if (argc == 1 && lisp_integer (argv[0], &count) == FALSE)
    return FALSE;
  status = forwline (argc == 1, count, KRANDOM);
  return status == TRUE ? TRUE : TINY_LISP_HOST_NIL;
}

static int
lisp_eobp (int argc, const char *const *argv,
           char *result, size_t result_size)
{
  (void) argv;
  (void) result;
  (void) result_size;

  if (argc != 0 || curwp == NULL || curbp == NULL)
    return FALSE;
  return curwp->w_dot.p == lastline (curbp)
           ? TRUE : TINY_LISP_HOST_NIL;
}

static int
lisp_region_append (char **buffer, size_t *used, size_t *size,
                    const char *text, size_t length)
{
  size_t needed;
  char *grown;

  if (length > SIZE_MAX - *used - 1)
    return FALSE;
  needed = *used + length + 1;
  if (needed > *size)
    {
      size_t new_size = *size;

      while (new_size < needed)
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
  memcpy (*buffer + *used, text, length);
  *used += length;
  (*buffer)[*used] = 0;
  return TRUE;
}

static char *
lisp_region_text (void)
{
  REGION region;
  LINE *line;
  int offset;
  char *buffer;
  size_t used = 0;
  size_t size = 256;

  if (getregion (&region) != TRUE)
    {
      lisp_message_displayed = TRUE;
      return NULL;
    }
  buffer = malloc (size);
  if (buffer == NULL)
    {
      eprintf ("[Lisp: region allocation failed]");
      lisp_message_displayed = TRUE;
      return NULL;
    }
  buffer[0] = 0;
  line = region.r_pos.p;
  offset = region.r_pos.o;
  while (region.r_size > 0)
    {
      const uchar *text;
      int chunk;
      int bytes;

      if (offset == wllength (line))
        {
          if (lisp_region_append (&buffer, &used, &size, "\n", 1)
              == FALSE)
            goto failed;
          line = lforw (line);
          offset = 0;
          region.r_size--;
          continue;
        }
      chunk = wllength (line) - offset;
      if (chunk > region.r_size)
        chunk = region.r_size;
      text = wlgetcptr (line, offset);
      bytes = unblen (text, chunk);
      if (bytes <= 0
          || lisp_region_append (&buffer, &used, &size,
                                 (const char *) text, (size_t) bytes)
             == FALSE)
        goto failed;
      offset += chunk;
      region.r_size -= chunk;
    }
  return buffer;

failed:
  free (buffer);
  eprintf ("[Lisp: region allocation failed]");
  lisp_message_displayed = TRUE;
  return NULL;
}

static int
lisp_switch_buffer (int argc, const char *const *argv,
                    char *result, size_t result_size)
{
  BUFFER *buffer;

  (void) result;
  (void) result_size;
  if (argc != 1 || curwp == NULL || argv[0][0] == 0
      || strlen (argv[0]) >= NBUFN)
    return FALSE;
  if ((buffer = bfind (argv[0], TRUE)) == NULL)
    return FALSE;
  return swbuffer (buffer);
}

static int
lisp_find_file (int argc, const char *const *argv,
                char *result, size_t result_size)
{
  char filename[NFILEN];
  char old_echo;
  int status;

  (void) result;
  (void) result_size;
  if (argc != 1 || argv[0][0] == 0
      || strlen (argv[0]) >= sizeof (filename))
    return FALSE;
  strcpy (filename, argv[0]);
  adjustcase (filename);
  old_echo = enoecho;
  enoecho = TRUE;
  status = visit_file (filename);
  enoecho = old_echo;
  return status;
}

static int
lisp_buffer_file_name (int argc, const char *const *argv,
                       char *result, size_t result_size)
{
  (void) argv;
  if (argc != 0 || curbp == NULL || curbp->b_fname[0] == 0)
    return FALSE;
  if (snprintf (result, result_size, "%s", curbp->b_fname)
      >= (int) result_size)
    return FALSE;
  return TINY_LISP_HOST_STRING;
}

static int
lisp_string_suffix_p (int argc, const char *const *argv,
                      char *result, size_t result_size)
{
  size_t text_length;
  size_t suffix_length;

  (void) result;
  (void) result_size;
  if (argc != 2)
    return FALSE;
  text_length = strlen (argv[0]);
  suffix_length = strlen (argv[1]);
  if (suffix_length > text_length)
    return TINY_LISP_HOST_NIL;
  if (memcmp (argv[0] + text_length - suffix_length, argv[1],
              suffix_length) != 0)
    return TINY_LISP_HOST_NIL;
  return TRUE;
}

static int
lisp_string_trim_suffix (int argc, const char *const *argv,
                         char *result, size_t result_size)
{
  size_t text_length;
  size_t suffix_length;
  size_t length;

  if (argc != 2)
    return FALSE;
  text_length = strlen (argv[0]);
  suffix_length = strlen (argv[1]);
  if (suffix_length <= text_length
      && memcmp (argv[0] + text_length - suffix_length, argv[1],
                 suffix_length) == 0)
    length = text_length - suffix_length;
  else
    length = text_length;
  if (length >= result_size)
    return FALSE;
  memcpy (result, argv[0], length);
  result[length] = 0;
  return TINY_LISP_HOST_STRING;
}

static int
lisp_join_path (int argc, const char *const *argv,
                char *result, size_t result_size)
{
  if (argc != 2 || argv[0][0] == 0 || argv[1][0] == 0)
    return FALSE;
  if (path_join (argv[0], argv[1], result, result_size) != TRUE)
    return FALSE;
  return TINY_LISP_HOST_STRING;
}

static int
lisp_resolve_path (int argc, const char *const *argv,
                   char *result, size_t result_size)
{
  char normalized[NFILEN];

  if (argc != 1 || argv[0][0] == 0
      || strlen (argv[0]) >= sizeof (normalized))
    return FALSE;
  strcpy (normalized, argv[0]);
  path_normalize (normalized);
  if (path_canonical_directory (normalized, result, result_size) != TRUE)
    return TINY_LISP_HOST_NIL;
  return TINY_LISP_HOST_STRING;
}

static int
lisp_set_buffer_readonly (int argc, const char *const *argv,
                          char *result, size_t result_size)
{
  int readonly;

  (void) result;
  (void) result_size;
  if (argc != 1 || curbp == NULL)
    return FALSE;
  readonly = argv[0][0] != 0 && strcmp (argv[0], "nil") != 0;
  if (readonly != FALSE)
    curbp->b_flag |= BFRO;
  else
    curbp->b_flag &= ~BFRO;
  updatemode ();
  return TRUE;
}

static int
lisp_create_mode (int argc, const char *const *argv,
                  char *result, size_t result_size)
{
  (void) result;
  (void) result_size;
  if (argc != 1 || argv[0][0] == 0 || curbp == NULL)
    return FALSE;
  createmode (argv[0]);
  return curbp->b_mode != NULL;
}

static int
lisp_read_directory_name (int argc, const char *const *argv,
                          char *result, size_t result_size)
{
  const char *default_directory = ".";
  char directory[NFILEN];
  char prompt[NFILEN];
  int *macro_pointer;
  int status;

  if ((argc != 1 && argc != 2) || argv[0][0] == 0
      || (argc == 2 && (argv[1][0] == 0
                        || strlen (argv[1]) >= sizeof (directory))))
    return FALSE;
  if (argc == 2)
    {
      default_directory = argv[1];
      if (snprintf (prompt, sizeof (prompt), "%s: ", argv[0])
          >= (int) sizeof (prompt))
        return FALSE;
    }
  else if (snprintf (prompt, sizeof (prompt), "%s: ", argv[0])
           >= (int) sizeof (prompt))
    return FALSE;

  if (scriptmode)
    strcpy (directory, default_directory);
  else
    {
      macro_pointer = kbdmop;
      if (macro_pointer != NULL)
        kbdmop = NULL;
      status = egetdname (prompt, directory, sizeof (directory));
      if (macro_pointer != NULL)
        kbdmop = macro_pointer;
      if (status == ABORT)
        {
          prompt_aborted = TRUE;
          return TINY_LISP_HOST_NIL;
        }
      if (status != TRUE || directory[0] == 0)
        strcpy (directory, default_directory);
    }
  if (snprintf (result, result_size, "%s", directory)
      >= (int) result_size)
    return FALSE;
  return TINY_LISP_HOST_STRING;
}

static int
lisp_read_file_name (int argc, const char *const *argv,
                     char *result, size_t result_size)
{
  const char *default_filename = "";
  char filename[NFILEN];
  char prompt[NFILEN];
  int *macro_pointer;
  int status;

  if ((argc != 1 && argc != 2) || argv[0][0] == 0
      || (argc == 2 && strlen (argv[1]) >= sizeof (filename)))
    return FALSE;
  if (argc == 2)
    {
      default_filename = argv[1];
      if (snprintf (prompt, sizeof (prompt), "%s (%s): ",
                    argv[0], default_filename) >= (int) sizeof (prompt))
        return FALSE;
    }
  else if (snprintf (prompt, sizeof (prompt), "%s: ", argv[0])
           >= (int) sizeof (prompt))
    return FALSE;

  if (scriptmode)
    strcpy (filename, default_filename);
  else
    {
      macro_pointer = kbdmop;
      if (macro_pointer != NULL)
        kbdmop = NULL;
      status = egetfname (prompt, filename, sizeof (filename));
      if (macro_pointer != NULL)
        kbdmop = macro_pointer;
      if (status == ABORT)
        {
          prompt_aborted = TRUE;
          return TINY_LISP_HOST_NIL;
        }
      if (status != TRUE)
        return FALSE;
    }
  if (filename[0] == 0)
    return TINY_LISP_HOST_NIL;
  if (snprintf (result, result_size, "%s", filename)
      >= (int) result_size)
    return FALSE;
  return TINY_LISP_HOST_STRING;
}

static void
lisp_dired_mode_string (unsigned int mode, char *result, size_t result_size)
{
  char *position = result;

  if (result_size < 11)
    return;
  *position++ = S_ISDIR (mode) != 0 ? 'd' : '-';
  *position++ = (mode & 0400) != 0 ? 'r' : '-';
  *position++ = (mode & 0200) != 0 ? 'w' : '-';
  *position++ = (mode & 0100) != 0 ? 'x' : '-';
  *position++ = (mode & 0040) != 0 ? 'r' : '-';
  *position++ = (mode & 0020) != 0 ? 'w' : '-';
  *position++ = (mode & 0010) != 0 ? 'x' : '-';
  *position++ = (mode & 0004) != 0 ? 'r' : '-';
  *position++ = (mode & 0002) != 0 ? 'w' : '-';
  *position++ = (mode & 0001) != 0 ? 'x' : '-';
  *position = 0;
}

static void
lisp_dired_timestamp (time_t timestamp, char *result, size_t result_size)
{
  struct tm *local_time;

  if (result_size < 17)
    return;
  local_time = localtime (&timestamp);
  if (local_time == NULL
      || strftime (result, result_size, "%Y-%m-%d %H:%M", local_time) == 0)
    strcpy (result, "-");
}

static int
lisp_dired_resolve_directory (const char *directory, char *result,
                              size_t result_size)
{
  char current[NFILEN];
  char posix_current[NFILEN];
  char normalized[NFILEN];
  int length;

  if (strlen (directory) >= sizeof (normalized))
    return FALSE;
  strcpy (normalized, directory);
  path_normalize (normalized);

  if (path_canonical_directory (normalized, result, result_size) == TRUE)
    return TRUE;
  if (normalized[0] == '~' || path_is_absolute (normalized))
    {
      length = snprintf (result, result_size, "%s", normalized);
      if (length < 0 || (size_t) length >= result_size)
        return FALSE;
      return TRUE;
    }
  if (getcwd (current, sizeof (current)) == NULL)
    {
      if (snprintf (result, result_size, "%s", normalized)
          >= (int) result_size)
        return FALSE;
      return TRUE;
    }
  path_to_posix (current, posix_current, sizeof (posix_current));
  if (normalized[0] == 0 || strcmp (normalized, ".") == 0)
    length = snprintf (result, result_size, "%s", posix_current);
  else
    return path_join (posix_current, normalized, result, result_size);
  if (length < 0 || (size_t) length >= result_size)
    return FALSE;
  return TRUE;
}

static int
lisp_dired_compare (const void *left, const void *right)
{
  const char *const *left_name = left;
  const char *const *right_name = right;

  return strcmp (*left_name, *right_name);
}

static int
lisp_dired_list (int argc, const char *const *argv,
                 char *result, size_t result_size)
{
  char expanded[NFILEN];
  char directory[NFILEN];
  char resolved[NFILEN];
  char native_directory[NFILEN];
  char **names = NULL;
  size_t count = 0;
  size_t capacity = 0;
  size_t index;
  DIR *stream;
  struct dirent *entry;
  int pass;
  int undo_started = FALSE;
  int status = FALSE;

  (void) result;
  (void) result_size;
  if (argc != 1 || argv[0][0] == 0 || curwp == NULL || curbp == NULL
      || strlen (argv[0]) >= sizeof (expanded))
    return FALSE;
  strcpy (expanded, argv[0]);
  if (snprintf (directory, sizeof (directory), "%s", fftilde (expanded))
      >= (int) sizeof (directory))
    return FALSE;
  if (directory[0] == 0)
    return FALSE;
  if (lisp_dired_resolve_directory (directory, resolved, sizeof (resolved))
      != TRUE)
    return FALSE;
  strcpy (directory, resolved);

  path_to_native (directory, native_directory, sizeof (native_directory));
  stream = opendir (native_directory);
  if (stream == NULL)
    {
      eprintf ("[Dired: cannot open %s]", directory);
      lisp_message_displayed = TRUE;
      return FALSE;
    }
  while ((entry = readdir (stream)) != NULL)
    {
      char **grown;

      if (count == capacity)
        {
          if (capacity == 0)
            capacity = 32;
          else if (capacity <= SIZE_MAX / 2)
            capacity *= 2;
          else
            goto done;
          grown = realloc (names, capacity * sizeof (*names));
          if (grown == NULL)
            goto done;
          names = grown;
        }
      names[count] = strdup (entry->d_name);
      if (names[count] == NULL)
        goto done;
      count++;
    }
  closedir (stream);
  stream = NULL;
  qsort (names, count, sizeof (*names), lisp_dired_compare);

  curbp->b_flag &= ~(BFCHG | BFRO);
  if (bclear (curbp) != TRUE || gotobob (FALSE, 1, KRANDOM) != TRUE)
    goto done;
  startsaveundo ();
  undo_started = TRUE;
  {
    char header[NFILEN + 6];
    int length;

    length = snprintf (header, sizeof (header), "%s:\n", directory);
    if (length < 0 || (size_t) length >= sizeof (header)
        || lisp_insert_once (header) == FALSE)
      goto done;
  }
  for (pass = 0; pass < 2; pass++)
    {
      char entry_path[NFILEN];
      char native_entry_path[NFILEN];
      char display[NFILEN + LISP_DIRED_ENTRY_COLUMN + 2];
      struct stat information = { 0 };
      char mode[11];
      char timestamp[17];
      int length;
      int is_directory;

      for (index = 0; index < count; index++)
        {
          if (strchr (names[index], '\n') != NULL
              || strlen (names[index]) >= NFILEN)
            continue;
          if (path_join (directory, names[index],
                         entry_path, sizeof (entry_path)) != TRUE)
            goto done;
          path_to_native (entry_path, native_entry_path,
                          sizeof (native_entry_path));
          if (stat (native_entry_path, &information) == 0)
            {
              is_directory = S_ISDIR (information.st_mode) != 0;
              lisp_dired_mode_string (information.st_mode, mode,
                                      sizeof (mode));
              lisp_dired_timestamp (information.st_mtime, timestamp,
                                    sizeof (timestamp));
            }
          else
            {
              is_directory = FALSE;
              strcpy (mode, "----------");
              strcpy (timestamp, "-");
            }
          if ((pass == 0) != (is_directory != 0))
            continue;
          length = snprintf (display, sizeof (display),
                             "%-10s %3llu %8lld %16s  %s%s\n",
                             mode,
                             (unsigned long long) information.st_nlink,
                             (long long) information.st_size,
                             timestamp, names[index],
                             is_directory ? "/" : "");
          if (length < 0 || (size_t) length >= sizeof (display)
              || lisp_insert_once (display) == FALSE)
            goto done;
        }
    }
  if (gotobob (FALSE, 1, KRANDOM) != TRUE)
    goto done;
  strcpy (curbp->b_fname, directory);
  curbp->b_flag &= ~(BFCHG | BFRO);
  status = TRUE;

done:
  if (undo_started)
    endsaveundo ();
  if (stream != NULL)
    closedir (stream);
  for (index = 0; index < count; index++)
    free (names[index]);
  free (names);
  return status;
}

static int
lisp_dired_entry_name (int argc, const char *const *argv,
                       char *result, size_t result_size)
{
  LINE *line;
  const uchar *text;
  int length;

  (void) argv;
  if (argc != 0 || curwp == NULL || curbp == NULL)
    return FALSE;
  line = curwp->w_dot.p;
  if (line == curbp->b_linep)
    return TINY_LISP_HOST_NIL;
  length = llength (line);
  text = lgets (line);
  if (length <= LISP_DIRED_ENTRY_COLUMN
      || (text[0] != 'd' && text[0] != '-'))
    return TINY_LISP_HOST_NIL;
  length -= LISP_DIRED_ENTRY_COLUMN;
  if ((size_t) length >= result_size)
    return FALSE;
  memcpy (result, text + LISP_DIRED_ENTRY_COLUMN, (size_t) length);
  result[length] = 0;
  return TINY_LISP_HOST_STRING;
}

static int
lisp_get_char (int argc, const char *const *argv,
               char *result, size_t result_size)
{
  LINE *line;
  const uchar *text;
  int length;

  (void) argv;
  if (argc != 0 || curwp == NULL || curbp == NULL)
    return FALSE;
  line = curwp->w_dot.p;
  if (line == curbp->b_linep
      || curwp->w_dot.o >= wllength (line))
    return TINY_LISP_HOST_NIL;

  text = wlgetcptr (line, curwp->w_dot.o);
  length = uclen (text);
  if (length <= 0 || (size_t) length >= result_size)
    return FALSE;
  memcpy (result, text, (size_t) length);
  result[length] = 0;
  return TINY_LISP_HOST_STRING;
}

static int
lisp_get_line (int argc, const char *const *argv,
               char *result, size_t result_size)
{
  LINE *line;
  int offset = 0;
  int length;

  if (argc > 1 || curwp == NULL || curbp == NULL)
    return FALSE;
  if (argc == 1 && lisp_integer (argv[0], &offset) == FALSE)
    return FALSE;
  line = curwp->w_dot.p;
  while (offset > 0 && line != curbp->b_linep)
    {
      line = lforw (line);
      offset--;
    }
  while (offset < 0 && line != curbp->b_linep)
    {
      line = lback (line);
      offset++;
    }
  if (offset != 0)
    return TINY_LISP_HOST_NIL;
  if (line == curbp->b_linep)
    return TINY_LISP_HOST_NIL;

  length = llength (line);
  if (length < 0 || (size_t) length >= result_size)
    return FALSE;
  memcpy (result, lgets (line), (size_t) length);
  result[length] = 0;
  return TINY_LISP_HOST_STRING;
}

static int
lisp_get_region (int argc, const char *const *argv,
                 char *result, size_t result_size)
{
  char *text;
  size_t length;

  (void) argv;
  if (argc != 0)
    return FALSE;
  text = lisp_region_text ();
  if (text == NULL)
    return TINY_LISP_HOST_NIL;
  length = strlen (text);
  if (length >= result_size)
    {
      free (text);
      eprintf ("[Lisp: region is too large]");
      lisp_message_displayed = TRUE;
      return TINY_LISP_HOST_NIL;
    }
  memcpy (result, text, length + 1);
  free (text);
  return TINY_LISP_HOST_STRING;
}

static int
lisp_insert_char (int argc, const char *const *argv,
                  char *result, size_t result_size)
{
  int character;
  int length;
  int status;

  (void) result;
  (void) result_size;
  if (argc != 1 || curwp == NULL)
    return FALSE;

  startsaveundo ();
  if (lisp_integer (argv[0], &character) == TRUE)
    {
      if (character <= 0 || character > 0x10ffff)
        {
          endsaveundo ();
          return FALSE;
        }
      status = linsert (1, character, NULL);
    }
  else
    {
      length = (int) strlen (argv[0]);
      if (length <= 0 || strchr (argv[0], '\n') != NULL
          || unslen ((const uchar *) argv[0], length) != 1)
        {
          endsaveundo ();
          return FALSE;
        }
      status = linsert (length, 0, (char *) argv[0]);
    }
  if (status == FALSE)
    {
      endsaveundo ();
      return FALSE;
    }
  endsaveundo ();
  return TRUE;
}

static int
lisp_save_buffer (int argc, const char *const *argv,
                  char *result, size_t result_size)
{
  (void) argv;
  (void) result;
  (void) result_size;

  if (argc != 0)
    return FALSE;
  return filesave (FALSE, 1, KRANDOM);
}

static int
lisp_kill_emacs (int argc, const char *const *argv,
                 char *result, size_t result_size)
{
  (void) argv;
  (void) result;
  (void) result_size;

  if (argc != 0)
    return FALSE;
  return quit (TRUE, 1, KRANDOM);
}

static int
lisp_define_command (int argc, const char *const *argv,
                     char *result, size_t result_size)
{
  int macro[NKBDM];
  char expression[LISP_EXPRESSION_SIZE];
  const char *command_expression;
  const char *text;
  size_t argument_length;
  size_t expression_length;
  size_t length = 0;

  (void) result;
  (void) result_size;
  if (argc != 2 || argv[0][0] == 0 || strlen (argv[0]) >= NXNAME
      || argv[1][0] == 0
      || kbdmip != NULL || kbdmop != NULL)
    return FALSE;

  argument_length = strlen (argv[1]);
  if (argv[1][0] == '(' && argv[1][argument_length - 1] == ')')
    {
      command_expression = argv[1];
      expression_length = argument_length;
    }
  else
    {
      if (argument_length >= sizeof (expression) - 2)
        return FALSE;
      expression[0] = '(';
      memcpy (expression + 1, argv[1], argument_length);
      expression[argument_length + 1] = ')';
      expression[argument_length + 2] = 0;
      command_expression = expression;
      expression_length = argument_length + 2;
    }
  if (expression_length > NKBDM - LISP_COMMAND_OVERHEAD)
    return FALSE;

  macro[length++] = KMETA | 'X';
  for (text = "eval-lisp"; *text != 0; text++)
    macro[length++] = *text;
  macro[length++] = 0;
  for (text = command_expression; *text != 0; text++)
    macro[length++] = *text;
  macro[length++] = 0;
  macro[length++] = KCTLX | ')';

  return symdefmacro (argv[0], macro, length);
}

#ifdef COLOR
static int
lisp_set_syntax (int argc, const char *const *argv,
                 char *result, size_t result_size)
{
  (void) result;
  (void) result_size;
  if (argc != 1 || curbp == NULL)
    return FALSE;
  if (colorflag == FALSE)
    return TRUE;
  if (syntax_set_buffer (curbp, argv[0]) == FALSE)
    return FALSE;
  syntax_mode_changed (curbp);
  return TRUE;
}

static int
lisp_set_modeline_color (int argc, const char *const *argv,
                         char *result, size_t result_size)
{
  int colors[4];
  int index;

  (void) result;
  (void) result_size;
  if (argc != 4)
    return FALSE;
  for (index = 0; index < 4; index++)
    if (lisp_integer (argv[index], &colors[index]) == FALSE
        || colors[index] < 0 || colors[index] > 255)
      return FALSE;

  modefg256 = colors[0];
  modebg256 = colors[1];
  modefg = colors[2];
  modebg = colors[3];
  ttsetmodecolor (modefg256, modebg256, modefg, modebg);
  updatemode ();
  return TRUE;
}
#endif

static int
lisp_append_text (char *result, size_t *length, size_t result_size,
                  const char *text, size_t text_length)
{
  if (*length + text_length >= result_size)
    return FALSE;
  memcpy (result + *length, text, text_length);
  *length += text_length;
  return TRUE;
}

static int
lisp_format_message (int argc, const char *const *argv,
                     char *result, size_t result_size)
{
  size_t length = 0;
  size_t index;
  int argument;

  if (argc < 1)
    return FALSE;
  argument = 1;
  for (index = 0; argv[0][index] != 0; index++)
    {
      char character = argv[0][index];

      if (character != '%')
        {
          if (lisp_append_text (result, &length, result_size,
                                &character, 1) == FALSE)
            return FALSE;
          continue;
        }
      index++;
      character = argv[0][index];
      if (character == '%')
        {
          if (lisp_append_text (result, &length, result_size,
                                "%", 1) == FALSE)
            return FALSE;
        }
      else if (character == 's' || character == 'S')
        {
          if (argument >= argc
              || lisp_append_text (result, &length, result_size,
                                   argv[argument],
                                   strlen (argv[argument])) == FALSE)
            return FALSE;
          argument++;
        }
      else if (character == 'd')
        {
          char number[32];
          int value;
          int number_length;

          if (argument >= argc
              || lisp_integer (argv[argument], &value) == FALSE)
            return FALSE;
          number_length = snprintf (number, sizeof (number), "%d", value);
          if (number_length < 0
              || (size_t) number_length >= sizeof (number)
              || lisp_append_text (result, &length, result_size,
                                   number, (size_t) number_length)
                 == FALSE)
            return FALSE;
          argument++;
        }
      else
        return FALSE;
    }
  if (argument != argc || length >= result_size)
    return FALSE;
  result[length] = 0;
  return TRUE;
}

static int
lisp_message_form (int argc, const char *const *argv,
                   char *result, size_t result_size)
{
  if (lisp_format_message (argc, argv, result, result_size) != TRUE)
    return FALSE;
  eprintf ("%s", result);
  lisp_message_displayed = TRUE;
  return TINY_LISP_HOST_STRING;
}

static int
lisp_set_debug_level (int argc, const char *const *argv,
                      char *result, size_t result_size)
{
  int level;

  (void) result;
  (void) result_size;
  if (argc != 1 || lisp_integer (argv[0], &level) == FALSE || level < 0)
    return FALSE;
  lisp_debug_level = level;
  return TRUE;
}

static int
lisp_debug_message (int argc, const char *const *argv,
                    char *result, size_t result_size)
{
  int level;

  if (argc < 2 || lisp_integer (argv[0], &level) == FALSE || level < 1
      || lisp_format_message (argc - 1, argv + 1, result, result_size)
         != TRUE)
    return FALSE;
  if (level <= lisp_debug_level)
    {
      eprintf ("[Lisp debug: %s]", result);
      lisp_message_displayed = TRUE;
    }
  return TRUE;
}

#ifdef AI
static int
lisp_getenv (int argc, const char *const *argv,
             char *result, size_t result_size)
{
  const char *value;

  if (argc != 1 || argv[0][0] == 0)
    return FALSE;
  value = getenv (argv[0]);
  if (value == NULL)
    return TINY_LISP_HOST_NIL;
  if (snprintf (result, result_size, "%s", value)
      >= (int) result_size)
    return FALSE;
  return TINY_LISP_HOST_STRING;
}
#endif

static int
lisp_set_variable (int argc, const char *const *argv,
                   char *result, size_t result_size)
{
  typedef struct LispVariable
  {
    const char *name;
    int *value;
  } LispVariable;

  static const LispVariable variables[] = {
    { "tab-size", &tabsize },
    { "tab-width", &tabsize },
    { "fill-column", &fillcol }
  };
  char *end;
  long value;
  size_t index;

  (void) result;
  (void) result_size;
  if (argc != 2)
    return FALSE;
  value = strtol (argv[1], &end, 10);
  if (argv[1][0] == 0 || *end != 0 || value <= 0 || value > INT_MAX)
    return FALSE;

  value = (int) value;
  for (index = 0; index < sizeof (variables) / sizeof (variables[0]);
       index++)
    if (strcmp (argv[0], variables[index].name) == 0)
      {
        *variables[index].value = value;
        return TRUE;
      }
  return FALSE;
}

#ifdef AI
static int
lisp_ai_set_endpoint (int argc, const char *const *argv,
                      char *result, size_t result_size)
{
  (void) result;
  (void) result_size;
  if (argc < 1 || argc > 2 || argv[0][0] == 0
      || (argc == 2 && argv[1][0] == 0))
    return FALSE;
  if (ai_configure (argv[0], argc == 2 ? argv[1] : NULL) != TRUE)
    {
      eprintf ("[AI: %s]", ai_last_error ());
      lisp_message_displayed = TRUE;
      return TINY_LISP_HOST_NIL;
    }
  return TRUE;
}

static int
lisp_ai_last_error (int argc, const char *const *argv,
                    char *result, size_t result_size)
{
  (void) argv;
  if (argc != 0)
    return FALSE;
  if (snprintf (result, result_size, "%s", ai_last_error ())
      >= (int) result_size)
    return FALSE;
  return TINY_LISP_HOST_STRING;
}

static int
lisp_ai_set_reasoning_effort (int argc, const char *const *argv,
                              char *result, size_t result_size)
{
  (void) result;
  (void) result_size;
  if (argc != 1)
    return FALSE;
  if (ai_set_reasoning_effort (argv[0]) != TRUE)
    {
      eprintf ("[AI: %s]", ai_last_error ());
      lisp_message_displayed = TRUE;
      return TINY_LISP_HOST_NIL;
    }
  return TRUE;
}

static int
lisp_ai_set_timeout (int argc, const char *const *argv,
                     char *result, size_t result_size)
{
  int seconds;

  (void) result;
  (void) result_size;
  if (argc != 1 || lisp_integer (argv[0], &seconds) == FALSE)
    return FALSE;
  if (ai_set_timeout (seconds) != TRUE)
    {
      eprintf ("[AI: %s]", ai_last_error ());
      lisp_message_displayed = TRUE;
      return TINY_LISP_HOST_NIL;
    }
  return TRUE;
}

static int
lisp_ai_complete (int argc, const char *const *argv,
                  char *result, size_t result_size)
{
  const char *model = NULL;
  int num_predict = 0;

  if (argc < 1 || argc > 3 || argv[0][0] == 0)
    return FALSE;
  if (argc >= 2 && argv[1][0] == 0)
    return FALSE;
  if (argc == 3
      && (lisp_integer (argv[2], &num_predict) == FALSE
          || num_predict <= 0))
    return FALSE;
  if (argc >= 2)
    model = argv[1];
  if (ai_complete (argv[0], model, num_predict,
                   result, result_size) != TRUE)
    {
      eprintf ("[AI: %s]", ai_last_error ());
      lisp_message_displayed = TRUE;
      return TINY_LISP_HOST_NIL;
    }
  return TINY_LISP_HOST_STRING;
}

static int
lisp_ai_complete_region (int argc, const char *const *argv,
                         char *result, size_t result_size)
{
  const char *model = NULL;
  char *prompt;
  int num_predict = 0;
  int status;

  if (argc > 2)
    return FALSE;
  if (argc >= 1 && argv[0][0] == 0)
    return FALSE;
  if (argc == 2
      && (lisp_integer (argv[1], &num_predict) == FALSE
          || num_predict <= 0))
    return FALSE;
  if (argc >= 1)
    model = argv[0];
  prompt = lisp_region_text ();
  if (prompt == NULL)
    return TINY_LISP_HOST_NIL;
  status = ai_complete (prompt, model, num_predict, result, result_size);
  free (prompt);
  if (status != TRUE)
    {
      eprintf ("[AI: %s]", ai_last_error ());
      lisp_message_displayed = TRUE;
      return TINY_LISP_HOST_NIL;
    }
  return TINY_LISP_HOST_STRING;
}
#endif

static int
lisp_register_hosts (void)
{
  typedef struct LispHost
  {
    const char *name;
    TinyLispHostFunction function;
  } LispHost;

  static const LispHost hosts[] = {
    { "define-key", lisp_bind_key },
    { "define-mode-key", lisp_bind_mode_key },
    { "set-variable", lisp_set_variable },
    { "message", lisp_message_form },
    { "load", lisp_load_file },
    { "require", lisp_require },
    { "set-lisp-debug", lisp_set_debug_level },
    { "debug-message", lisp_debug_message },
    { "execute-command", lisp_execute_command },
    { "editor-cmd", lisp_execute_command },
    { "get-char", lisp_get_char },
    { "me-find-file", lisp_find_file },
    { "insert-char", lisp_insert_char },
    { "get-line", lisp_get_line },
    { "get-region", lisp_get_region },
    { "save-buffer", lisp_save_buffer },
    { "kill-emacs", lisp_kill_emacs },
    { "insert-string", lisp_insert_string },
    { "goto-bol", lisp_goto_bol },
    { "goto-eol", lisp_goto_eol },
    { "forward-line", lisp_forward_line },
    { "eobp", lisp_eobp },
    { "define-lisp-command", lisp_define_command },
    { "switch-to-buffer", lisp_switch_buffer },
    { "buffer-file-name", lisp_buffer_file_name },
    { "string-suffix-p", lisp_string_suffix_p },
    { "string-trim-suffix", lisp_string_trim_suffix },
    { "join-path", lisp_join_path },
    { "resolve-path", lisp_resolve_path },
    { "read-directory-name", lisp_read_directory_name },
    { "read-file-name", lisp_read_file_name },
    { "set-buffer-readonly", lisp_set_buffer_readonly },
    { "create-mode", lisp_create_mode },
    { "dired-list", lisp_dired_list },
    { "dired-entry-name", lisp_dired_entry_name },
#ifdef AI
    { "getenv", lisp_getenv },
    { "ai-set-endpoint", lisp_ai_set_endpoint },
    { "ai-set-reasoning-effort", lisp_ai_set_reasoning_effort },
    { "ai-set-timeout", lisp_ai_set_timeout },
    { "ai-complete", lisp_ai_complete },
    { "ai-complete-region", lisp_ai_complete_region },
    { "ai-last-error", lisp_ai_last_error },
#endif
#ifdef COLOR
    { "set-modeline-color", lisp_set_modeline_color },
    { "set-syntax", lisp_set_syntax }
#endif
  };
  static int registered = FALSE;
  size_t index;

  if (registered == TRUE)
    return TRUE;
  for (index = 0; index < sizeof (hosts) / sizeof (hosts[0]); index++)
    if (tiny_lisp_define_host (hosts[index].name,
                               hosts[index].function) != 1)
      return FALSE;
  registered = TRUE;
  return TRUE;
}

static int
lisp_true_argument (const char *argument)
{
  return argument != NULL && argument[0] != 0
         && strcmp (argument, "nil") != 0;
}

static int
lisp_valid_module_name (const char *name)
{
  return name != NULL && name[0] != 0
         && path_has_separator (name) == FALSE
         && strchr (name, ':') == NULL;
}

static int
lisp_module_loaded (const char *name)
{
  size_t index;

  for (index = 0; index < lisp_loaded_module_count; index++)
    if (strcmp (lisp_loaded_modules[index], name) == 0)
      return TRUE;
  return FALSE;
}

static int
lisp_mark_module_loaded (const char *name)
{
  char **modules;
  char *copy;
  size_t length;

  if (lisp_module_loaded (name) == TRUE)
    return TRUE;
  if (lisp_loaded_module_count == lisp_loaded_module_capacity)
    {
      size_t capacity = lisp_loaded_module_capacity == 0
                        ? 8 : lisp_loaded_module_capacity * 2;

      modules = realloc (lisp_loaded_modules,
                         capacity * sizeof (*modules));
      if (modules == NULL)
        return FALSE;
      lisp_loaded_modules = modules;
      lisp_loaded_module_capacity = capacity;
    }

  length = strlen (name);
  copy = malloc (length + 1);
  if (copy == NULL)
    return FALSE;
  memcpy (copy, name, length + 1);
  lisp_loaded_modules[lisp_loaded_module_count++] = copy;
  return TRUE;
}

static int
lisp_has_suffix (const char *filename, const char *suffix)
{
  size_t filename_length = strlen (filename);
  size_t suffix_length = strlen (suffix);
  size_t offset;

  if (filename_length < suffix_length)
    return FALSE;
  filename += filename_length - suffix_length;
  for (offset = 0; offset < suffix_length; offset++)
    if (tolower ((unsigned char) filename[offset]) != suffix[offset])
      return FALSE;
  return TRUE;
}

int
lispfile (const char *filename)
{
  return filename != NULL
         && (lisp_has_suffix (filename, ".l")
             || lisp_has_suffix (filename, ".lisp")
             || lisp_has_suffix (filename, ".mel"));
}

static int
lisp_open_file (const char *filename, FILE **file)
{
  char expanded[NFILEN];
  char native[NFILEN];

  if (filename == NULL)
    {
      *file = fopen (LISP_CONFIG_FILE, "rb");
      if (*file != NULL)
        return TRUE;
      if (snprintf (expanded, sizeof (expanded), "~/%s",
                    LISP_CONFIG_FILE) >= (int) sizeof (expanded))
        return FALSE;
    }
  else
    {
      if (snprintf (expanded, sizeof (expanded), "%s",
                    filename) >= (int) sizeof (expanded))
        return FALSE;
    }

  filename = fftilde (expanded);
  path_to_native (filename, native, sizeof (native));
  *file = fopen (native, "rb");
  return *file != NULL;
}

static int
lisp_read_file (const char *filename, char **source)
{
  FILE *file;
  long length;

  if (lisp_open_file (filename, &file) != TRUE)
    return FALSE;
  if (fseek (file, 0, SEEK_END) != 0 || (length = ftell (file)) < 0)
    {
      fclose (file);
      return FALSE;
    }
  rewind (file);
  *source = malloc ((size_t) length + 1);
  if (*source == NULL
      || fread (*source, 1, (size_t) length, file) != (size_t) length)
    {
      free (*source);
      *source = NULL;
      fclose (file);
      return FALSE;
    }
  (*source)[length] = 0;
  fclose (file);
  return TRUE;
}

static int
lisp_is_explicit_path (const char *filename)
{
  if (filename[0] == '~' || path_is_absolute (filename))
    return TRUE;
  return path_has_separator (filename);
}

static int
lisp_read_search_file (const char *filename, char **source,
                       char *resolved, size_t resolved_size)
{
  static const char *const paths[] = {
    "./",
    "~/.mel.d/",
    "~/.config/me/lisp/",
    "/usr/local/share/me/lisp/",
    "/usr/share/me/lisp/"
  };
  char candidate[NFILEN];
  size_t index;

  *source = NULL;
  if (filename == NULL)
    return lisp_read_file (filename, source);

  if (lisp_is_explicit_path (filename) == TRUE)
    {
      if (lisp_read_file (filename, source) != TRUE)
        return FALSE;
      if (snprintf (resolved, resolved_size, "%s", filename)
          >= (int) resolved_size)
        {
          free (*source);
          *source = NULL;
          return FALSE;
        }
      return TRUE;
    }

  for (index = 0; index < sizeof (paths) / sizeof (paths[0]); index++)
    {
      if (snprintf (candidate, sizeof (candidate), "%s%s",
                    paths[index], filename) >= (int) sizeof (candidate))
        continue;
      if (lisp_read_file (candidate, source) != TRUE)
        continue;
      if (snprintf (resolved, resolved_size, "%s", candidate)
          >= (int) resolved_size)
        {
          free (*source);
          *source = NULL;
          return FALSE;
        }
      return TRUE;
    }
  return FALSE;
}

static int
lisp_evaluate_source (char *source, int report)
{
  char result[LISP_OUTPUT_SIZE];
  int status;

  lisp_message_displayed = FALSE;
  prompt_aborted = FALSE;
  status = tiny_lisp_eval_source (source, result, sizeof (result));
  free (source);
  if (status != TRUE)
    eprintf ("[Lisp error: %s]", result);
  else if (prompt_aborted != FALSE)
    show_prompt_abort ();
  else if (report != FALSE && lisp_message_displayed == FALSE
           && (lisp_debug_level > 0 || strcmp (result, "t") != 0))
    eprintf ("[Lisp: %s]", result);
  return status;
}

static void
show_prompt_abort (void)
{
  ttmove (ttrow, prompt_abort_column ());
  eputs ("^G");
  tteeol ();
  ttflush ();
  epresf = TRUE;
}

static int
prompt_abort_column (void)
{
  return ttcol - 2 >= 0 ? ttcol - 2 : 0;
}

static int
lisp_load_source (const char *filename, int report, int quiet_missing)
{
  char resolved[NFILEN];
  char *source;

  if (lisp_register_hosts () != TRUE)
    {
      eprintf ("[Lisp host commands unavailable]");
      return FALSE;
    }

  if (filename == NULL)
    {
      if (lisp_read_file (NULL, &source) != TRUE)
        return TRUE;
      return lisp_evaluate_source (source, report);
    }

  if (lisp_read_search_file (filename, &source,
                             resolved, sizeof (resolved)) != TRUE)
    {
      if (quiet_missing == FALSE)
        eprintf ("Unable to open Lisp file %s", filename);
      return FALSE;
    }
  return lisp_evaluate_source (source, report);
}

static int
lispload (const char *filename, int report)
{
  return lisp_load_source (filename, report, FALSE);
}

static int
lisp_load_file (int argc, const char *const *argv,
                char *result, size_t result_size)
{
  (void) result;
  (void) result_size;
  if (argc != 1 || argv[0][0] == 0)
    return FALSE;
  return lispload (argv[0], FALSE);
}

static int
lisp_module_filename (const char *name, const char *suffix,
                      char *filename, size_t filename_size)
{
  return snprintf (filename, filename_size, "%s%s", name, suffix)
         < (int) filename_size;
}

static int
lisp_require (int argc, const char *const *argv,
              char *result, size_t result_size)
{
  static const char *const suffixes[] = { ".l", ".lisp", ".mel" };
  const char *filename = NULL;
  char resolved[NFILEN];
  char module_filename[NFILEN];
  char *source;
  int noerror = FALSE;
  size_t index;

  (void) result;
  (void) result_size;
  if (argc < 1 || argc > 3
      || lisp_valid_module_name (argv[0]) != TRUE)
    return FALSE;
  if (argc >= 2 && argv[1][0] != 0 && strcmp (argv[1], "nil") != 0)
    filename = argv[1];
  if (argc == 3)
    noerror = lisp_true_argument (argv[2]);
  if (lisp_module_loaded (argv[0]) == TRUE)
    return TRUE;
  if (lisp_register_hosts () != TRUE)
    return FALSE;

  if (filename != NULL)
    {
      if (lisp_read_search_file (filename, &source,
                                 resolved, sizeof (resolved)) != TRUE)
        return noerror ? TINY_LISP_HOST_NIL : FALSE;
      if (lisp_evaluate_source (source, FALSE) != TRUE)
        return FALSE;
      return lisp_mark_module_loaded (argv[0]) == TRUE
             ? TRUE : FALSE;
    }

  for (index = 0; index < sizeof (suffixes) / sizeof (suffixes[0]);
       index++)
    {
      if (lisp_module_filename (argv[0], suffixes[index],
                                module_filename,
                                sizeof (module_filename)) != TRUE)
        return FALSE;
      if (lisp_read_search_file (module_filename, &source,
                                 resolved, sizeof (resolved)) != TRUE)
        continue;
      if (lisp_evaluate_source (source, FALSE) != TRUE)
        return FALSE;
      return lisp_mark_module_loaded (argv[0]) == TRUE ? TRUE : FALSE;
    }

  return noerror ? TINY_LISP_HOST_NIL : FALSE;
}

int
lispstartup (const char *filename)
{
  return lispload (filename, FALSE);
}

int
lisprun (const char *filename)
{
  return lispload (filename, TRUE);
}

int
lispevalfile (int f, int n, int k)
{
  char filename[NFILEN];
  int status;

  (void) f;
  (void) n;
  (void) k;

  status = egetfname ("Read Lisp: ", filename, NFILEN);
  if (status != TRUE)
    return status;
  adjustcase (filename);
  return lisprun (filename);
}

int
lispeval (int f, int n, int k)
{
  char expr[LISP_EXPRESSION_SIZE];
  char source[LISP_EXPRESSION_SIZE + 64];
  char result[LISP_OUTPUT_SIZE];
  int prefix;
  int status;

  (void) k;

  if (ereadv ("Lisp: ", expr, sizeof (expr), EFNEW | EFCR) != TRUE)
    return FALSE;
  if (expr[0] == 0)
    return FALSE;
  if (lisp_register_hosts () != TRUE)
    {
      eprintf ("[Lisp host commands unavailable]");
      return FALSE;
    }

  prefix = f != FALSE ? n : 1;
  if (snprintf (source, sizeof (source),
                "(progn (setq prefix-arg %d) %s)", prefix, expr)
      >= (int) sizeof (source))
    return FALSE;

  lisp_message_displayed = FALSE;
  prompt_aborted = FALSE;
  status = tiny_lisp_eval (source, result, sizeof (result));
  if (status != TRUE)
    eprintf ("[Lisp error: %s]", result);
  else if (prompt_aborted != FALSE)
    show_prompt_abort ();
  else if (lisp_message_displayed == FALSE
           && (lisp_debug_level > 0 || strcmp (result, "t") != 0))
    eprintf ("[Lisp: %s]", result);
  return status;
}

#endif
