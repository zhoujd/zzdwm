/*
 * Filename completion.c
 */

#include "def.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

#if defined (_WIN32) || defined (__MINGW32__)
# include <io.h>
# include <windows.h>
# define TMPBUF_SIZE MAX_PATH
#else
# include <unistd.h>
# include <limits.h>
# define TMPBUF_SIZE PATH_MAX
#endif

/* Single universal path separator across all platforms */
#define PATH_SEP '/'

/* Maximum matches to hold in memory during completion */
#define MAX_MATCHES 256

/* Forward declarations */
static void outstring (char *s);
static void sanitize_slashes (char *path);
static void normalize_path (char *path);
static int collect_matches (const char *prefix,
                            char matches[][TMPBUF_SIZE],
                            int max_matches);

/*
 * Helper: Converts backslashes to forward slashes, translates Windows drive
 * letters (e.g. "C:/abc" -> "/c/abc"), and collapses redundant slashes.
 */
static void
sanitize_slashes (char *path)
{
  char *src = path;
  char *dst = path;
  char temp[TMPBUF_SIZE];

  if (path == NULL || *path == '\0')
    return;

  /* First pass: Normalize all backslashes to forward slashes */
  for (char *p = path; *p; p++)
    {
      if (*p == '\\')
        *p = '/';
    }

  /*
   * Convert Windows Drive Letter ("C:/" or "C:") to MSYS style ("/c/" or "/c")
   */
  if (isalpha ((unsigned char)path[0]) && path[1] == ':')
    {
      char drive = (char) tolower ((unsigned char)path[0]);
      if (path[2] == '/' || path[2] == '\0')
        {
          snprintf (temp, TMPBUF_SIZE, "/%c%s", drive, path + 2);
          strncpy (path, temp, TMPBUF_SIZE - 1);
          path[TMPBUF_SIZE - 1] = '\0';
        }
    }

  src = path;

  /* Preserve Windows UNC network path prefix (e.g., "//server/share") */
  if (src[0] == '/' && src[1] == '/')
    {
      *dst++ = '/';
      *dst++ = '/';
      src += 2;
    }

  while (*src)
    {
      char c = *src;

      /* Collapse duplicate slashes */
      if (c == '/' && dst > path && dst[-1] == '/')
        {
          src++;
          continue;
        }

      *dst++ = c;
      src++;
    }

  *dst = '\0';
}

/*
 * Helper: Simplifies path components by resolving '.' and '..'
 * in-place within the buffer.
 */
static void
normalize_path (char *path)
{
  char *src = path;
  char *dst = path;
  int is_abs = (path[0] == '/');

  /* Preserve UNC prefix if present */
  if (path[0] == '/' && path[1] == '/')
    {
      src += 2;
      dst += 2;
      is_abs = 1;
    }
  else if (is_abs)
    {
      src++;
      dst++;
    }

  while (*src)
    {
      if (*src == '/')
        {
          src++;
          continue;
        }

      /* Check for '.' component */
      if (src[0] == '.' && (src[1] == '/' || src[1] == '\0'))
        {
          src += (src[1] == '\0') ? 1 : 2;
          continue;
        }

      /* Check for '..' component */
      if (src[0] == '.' && src[1] == '.' && (src[2] == '/' || src[2] == '\0'))
        {
          src += (src[2] == '\0') ? 2 : 3;

          /* Rewind dst to previous directory separator */
          if (dst > path + (is_abs ? 1 : 0))
            {
              if (dst[-1] == '/')
                dst--;

              while (dst > path + (is_abs ? 1 : 0) && dst[-1] != '/')
                dst--;
            }
          continue;
        }

      /* Append directory separator before regular path component */
      if (dst != path && dst[-1] != '/')
        *dst++ = PATH_SEP;

      while (*src && *src != '/')
        *dst++ = *src++;
    }

  /* Preserve trailing slash if original path ended with a separator */
  if (src > path && src[-1] == '/' && dst > path && dst[-1] != PATH_SEP)
    *dst++ = PATH_SEP;

  *dst = '\0';

  /* Fallback to current directory if path collapses completely */
  if (path[0] == '\0')
    strcpy (path, ".");
}

/*
 * Helper: Perform in-memory directory scanning for filename matching.
 */
static int
collect_matches (const char *prefix,
                 char matches[][TMPBUF_SIZE],
                 int max_matches)
{
  char dirpath[TMPBUF_SIZE];
  char pattern[TMPBUF_SIZE];
  char expanded_prefix[TMPBUF_SIZE];
  int match_count = 0;
  const char *last_slash;
  DIR *dir;
  struct dirent *entry;
  size_t pat_len;

  /* 1. Copy raw input */
  strncpy (expanded_prefix, prefix, TMPBUF_SIZE - 1);
  expanded_prefix[TMPBUF_SIZE - 1] = '\0';

  /* 2. Sanitize separators and convert Windows drives to /c/ style */
  sanitize_slashes (expanded_prefix);

  /* 3. Handle ~ expansion */
  if (expanded_prefix[0] == '~')
    {
      const char *home = NULL;

#if defined (_WIN32) || defined (__MINGW32__)
      home = getenv ("HOME");
      if (home == NULL)
        home = getenv ("USERPROFILE");
#else
      home = getenv ("HOME");
#endif

      if (home != NULL)
        {
          if (expanded_prefix[1] == '/' || expanded_prefix[1] == '\0')
            {
              char temp[TMPBUF_SIZE];
              snprintf (temp, TMPBUF_SIZE, "%s%s", home, expanded_prefix + 1);
              strncpy (expanded_prefix, temp, TMPBUF_SIZE - 1);
              expanded_prefix[TMPBUF_SIZE - 1] = '\0';
              sanitize_slashes (expanded_prefix);
            }
        }
    }

  /* 4. Collapse '.' and '..' path components */
  normalize_path (expanded_prefix);

  strcpy (dirpath, ".");
  pattern[0] = '\0';

  /* 5. Separate directory path from search pattern prefix */
  last_slash = strrchr (expanded_prefix, '/');

  if (last_slash != NULL)
    {
      size_t dirlen = (size_t) (last_slash - expanded_prefix);
      if (dirlen == 0)
        strcpy (dirpath, "/");
      else
        {
          if (dirlen >= TMPBUF_SIZE)
            dirlen = TMPBUF_SIZE - 1;
          strncpy (dirpath, expanded_prefix, dirlen);
          dirpath[dirlen] = '\0';
        }
      strcpy (pattern, last_slash + 1);
    }
  else
    strcpy (pattern, expanded_prefix);

  dir = opendir (dirpath);

  /* Fallback: If opendir fails on Windows POSIX path like "/c", attempt "C:/" */
  if (dir == NULL && dirpath[0] == '/' && isalpha ((unsigned char)dirpath[1]) && dirpath[2] == '\0')
    {
      char win_drive[4] = { dirpath[1], ':', '/', '\0' };
      dir = opendir (win_drive);
    }

  if (dir == NULL)
    return 0;

  pat_len = strlen (pattern);

  while ((entry = readdir (dir)) != NULL && match_count < max_matches)
    {
      /* Skip current/parent directory links */
      if (strcmp (entry->d_name, ".") == 0
          || strcmp (entry->d_name, "..") == 0)
        continue;

      /* Match prefix (case-insensitive on Windows) */
#if defined (_WIN32) || defined (__MINGW32__)
      if (pat_len == 0 || _strnicmp (entry->d_name, pattern, pat_len) == 0)
#else
      if (pat_len == 0 || strncmp (entry->d_name, pattern, pat_len) == 0)
#endif
        {
          struct stat st;
          int ret;

          if (last_slash != NULL)
            {
              size_t orig_dirlen = (size_t) (last_slash - expanded_prefix);
              if (orig_dirlen > TMPBUF_SIZE - 256)
                orig_dirlen = TMPBUF_SIZE - 256;

              ret = snprintf (matches[match_count], TMPBUF_SIZE, "%.*s/%s",
                              (int) orig_dirlen, expanded_prefix, entry->d_name);
            }
          else
            ret = snprintf (matches[match_count], TMPBUF_SIZE, "%s",
                            entry->d_name);

          if (ret < 0 || ret >= TMPBUF_SIZE)
            continue;

          /* Perform stat check to append trailing slash for directories */
          char full_check_path[TMPBUF_SIZE];
          ret = snprintf (full_check_path, TMPBUF_SIZE, "%s/%s", dirpath, entry->d_name);
          if (ret < 0 || ret >= TMPBUF_SIZE)
            continue;

          if (stat (full_check_path, &st) == 0 && S_ISDIR (st.st_mode))
            {
              size_t len = strlen (matches[match_count]);
              if (len < TMPBUF_SIZE - 2)
                {
                  matches[match_count][len] = PATH_SEP;
                  matches[match_count][len + 1] = '\0';
                }
            }

          match_count++;
        }
    }

  closedir (dir);
  return match_count;
}

/*
 * Basic filename completion
 */
int
getfilename (char *prompt, char *buf, int nbuf)
{
  int cpos = 0;
  int c;
  int eolchar = '\n';

  static char matches[MAX_MATCHES][TMPBUF_SIZE];
  int match_count = 0;
  int match_idx = -1;

  eprintf (prompt);

  for (;;)
    {
      c = ttgetc ();

      if (c == CCHR ('M'))
        c = '\n';

      if (c != 0x09 && c != ' ' && c != '?')
        {
          match_count = 0;
          match_idx = -1;
        }

      if (c == eolchar)
        {
          buf[cpos] = '\0';
          eerase ();
          ttflush ();

          if (buf[0] == '\0')
            return FALSE;

          return TRUE;
        }

      if (c == CCHR ('G'))
        {
          ctrlg (FALSE, 0, KRANDOM);
          eputc (c);
          ttflush ();
          return ABORT;
        }

      else if (c == 0x7F || c == 0x08 || c == 0x107)
        {
          if (cpos != 0)
            {
              outstring ("\b \b");
              --ttcol;
              if (buf[--cpos] < 0x20)
                {
                  outstring ("\b \b");
                  --ttcol;
                }
              if (buf[cpos] == '\n')
                {
                  outstring ("\b\b  \b\b");
                  ttcol -= 2;
                }
              ttflush ();
            }
        }

      else if (c == 0x15)
        {
          while (cpos != 0)
            {
              outstring ("\b \b");
              --ttcol;

              if (buf[--cpos] < 0x20)
                {
                  outstring ("\b \b");
                  --ttcol;
                }
              if (buf[cpos] == '\n')
                {
                  outstring ("\b\b  \b\b");
                  ttcol -= 2;
                }
            }
          ttflush ();
        }

      else if (c == 0x09 || c == ' ' || c == '?')
        {
          buf[cpos] = '\0';

          if (match_count == 0)
            {
              match_count = collect_matches (buf, matches, MAX_MATCHES);
              match_idx = 0;
            }
          else
            {
              match_idx = (match_idx + 1) % match_count;
            }

          if (match_count > 0)
            {
              int n;

              while (cpos > 0)
                {
                  outstring ("\b \b");
                  --ttcol;
                  cpos--;
                }

              strncpy (buf, matches[match_idx], (size_t) (nbuf - 1));
              buf[nbuf - 1] = '\0';

              if (match_count == 1)
                {
                  size_t len = strlen (buf);
                  if (len > 0 && buf[len - 1] == PATH_SEP)
                    {
                      match_count = 0;
                      match_idx = -1;
                    }
                }

              cpos = (int) strlen (buf);

              for (n = 0; n < cpos; n++)
                {
                  c = buf[n];
                  if ((c < ' ') && (c != '\n'))
                    {
                      outstring ("^");
                      ++ttcol;
                      c ^= 0x40;
                    }

                  if (c != '\n')
                    ttputc (c);
                  else
                    {
                      outstring ("<NL>");
                      ttcol += 3;
                    }
                  ++ttcol;
                }
              ttflush ();
            }
          else
            ttbeep ();
        }

      else
        {
          if (cpos < nbuf - 1)
            {
              if ((c < ' ') && (c != '\n'))
                ttbeep ();
              else
                {
                  buf[cpos++] = c;
                  ttputc (c);
                  ++ttcol;
                  ttflush ();
                }
            }
        }
    }
}

void
outstring (char *s)
{
  while (*s)
    ttputc (*s++);
}
