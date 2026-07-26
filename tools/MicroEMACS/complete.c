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
static void to_native_path (const char *posix_path, char *native_path, size_t max_len);
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
  char *src;
  char *dst;
  char temp[TMPBUF_SIZE];

  if (path == NULL || *path == '\0')
    return;

  /* First pass: Normalize all backslashes to forward slashes */
  for (char *p = path; *p; p++)
    {
      if (*p == '\\')
        *p = '/';
    }

  /* Convert Windows Drive Letter ("C:/" or "C:") to MSYS style ("/c/" or "/c") */
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
  dst = path;

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
 * in-place within the buffer while protecting drive/root prefixes.
 */
static void
normalize_path (char *path)
{
  char *src;
  char *dst;
  int root_len = 0;

  if (path == NULL || *path == '\0')
    return;

  /* 1. Calculate and protect root prefix length */
  if (path[0] == '/' && path[1] == '/')
    {
      /* UNC path: //server/share */
      root_len = 2;
    }
  else if (path[0] == '/' && isalpha ((unsigned char)path[1]) &&
           (path[2] == '/' || path[2] == '\0'))
    {
      /* MSYS style drive prefix: /c/ or /c */
      root_len = (path[2] == '/') ? 3 : 2;
    }
  else if (path[0] == '/')
    {
      /* POSIX root: / */
      root_len = 1;
    }

  src = path + root_len;
  dst = path + root_len;

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

          /* Rewind dst, but NEVER past the protected root_len */
          if (dst > path + root_len)
            {
              if (dst[-1] == '/')
                dst--;

              while (dst > path + root_len && dst[-1] != '/')
                dst--;
            }
          continue;
        }

      /* Append directory separator before regular path component */
      if (dst > path && dst[-1] != '/')
        *dst++ = PATH_SEP;

      while (*src && *src != '/')
        *dst++ = *src++;
    }

  /* Preserve trailing slash if original path ended with a separator */
  if (src > path && src[-1] == '/' && dst > path + root_len && dst[-1] != PATH_SEP)
    *dst++ = PATH_SEP;

  *dst = '\0';

  /* Fallback to current directory if path collapses completely */
  if (path[0] == '\0')
    strcpy (path, ".");
}

/*
 * Helper: Converts a POSIX MSYS-style path (/c/path) to a Windows native path (C:/path)
 * solely for filesystem syscalls (opendir, stat) on Win32 builds.
 */
static void
to_native_path (const char *posix_path, char *native_path, size_t max_len)
{
  strncpy (native_path, posix_path, max_len - 1);
  native_path[max_len - 1] = '\0';

#if defined (_WIN32) || defined (__MINGW32__)
  if (native_path[0] == '/' && isalpha ((unsigned char)native_path[1]) &&
      (native_path[2] == '/' || native_path[2] == '\0'))
    {
      char drive = native_path[1];
      char temp[TMPBUF_SIZE];

      if (native_path[2] == '\0')
        snprintf (temp, TMPBUF_SIZE, "%c:/", drive);
      else
        snprintf (temp, TMPBUF_SIZE, "%c:%s", drive, native_path + 2);

      strncpy (native_path, temp, max_len - 1);
      native_path[max_len - 1] = '\0';
    }
#endif
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
  char native_dirpath[TMPBUF_SIZE];
  int match_count = 0;
  const char *last_slash;
  DIR *dir;
  struct dirent *entry;
  size_t pat_len;

  /* 1. Copy raw input */
  strncpy (expanded_prefix, prefix, TMPBUF_SIZE - 1);
  expanded_prefix[TMPBUF_SIZE - 1] = '\0';

  /* 2. Sanitize all separators to forward slashes */
  sanitize_slashes (expanded_prefix);

  /* 3. Handle ~ expansion using $HOME */
  if (expanded_prefix[0] == '~')
    {
      const char *home = NULL;

#if defined (_WIN32) || defined (__MINGW32__)
      home = getenv ("HOME");
      if (home == NULL)
        home = getenv ("USERPROFILE");
      if (home == NULL)
        {
          static char win_home[TMPBUF_SIZE];
          const char *drive = getenv ("HOMEDRIVE");
          const char *path = getenv ("HOMEPATH");
          if (drive != NULL && path != NULL)
            {
              snprintf (win_home, TMPBUF_SIZE, "%s%s", drive, path);
              home = win_home;
            }
        }
#else
      home = getenv ("HOME");
#endif

      if (home != NULL)
        {
          if (expanded_prefix[1] == '/' || expanded_prefix[1] == '\0')
            {
              char temp[TMPBUF_SIZE];
              size_t home_len = strlen (home);

              if (home_len < TMPBUF_SIZE)
                {
                  /* Copy home directory safely */
                  memcpy (temp, home, home_len);
                  /* Append remaining path after '~' */
                  strncpy (temp + home_len, expanded_prefix + 1, TMPBUF_SIZE - home_len - 1);
                  temp[TMPBUF_SIZE - 1] = '\0';

                  strncpy (expanded_prefix, temp, TMPBUF_SIZE - 1);
                  expanded_prefix[TMPBUF_SIZE - 1] = '\0';

                  /* Normalize slashes after ~ replacement */
                  sanitize_slashes (expanded_prefix);
                }

              /* Normalize slashes after ~ replacement */
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

  /* Convert POSIX dirpath to Windows Native path (/c/zznix -> C:/zznix) for opendir */
  to_native_path (dirpath, native_dirpath, TMPBUF_SIZE);

  dir = opendir (native_dirpath);
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

          /* Rebuild full matched display path cleanly; prevent double slashes at root */
          if (last_slash != NULL)
            {
              if (strcmp (dirpath, "/") == 0)
                ret = snprintf (matches[match_count], TMPBUF_SIZE, "/%s", entry->d_name);
              else
                ret = snprintf (matches[match_count], TMPBUF_SIZE, "%s/%s", dirpath, entry->d_name);
            }
          else
            ret = snprintf (matches[match_count], TMPBUF_SIZE, "%s", entry->d_name);

          if (ret < 0 || ret >= TMPBUF_SIZE)
            continue;

          /* Perform stat check to append trailing slash for directories */
          char full_check_path[TMPBUF_SIZE];
          char native_full_check[TMPBUF_SIZE];

          if (strcmp (dirpath, "/") == 0)
            ret = snprintf (full_check_path, TMPBUF_SIZE, "/%s", entry->d_name);
          else
            ret = snprintf (full_check_path, TMPBUF_SIZE, "%s/%s", dirpath, entry->d_name);

          if (ret < 0 || ret >= TMPBUF_SIZE)
            continue;

          to_native_path (full_check_path, native_full_check, TMPBUF_SIZE);

          if (stat (native_full_check, &st) == 0 && S_ISDIR (st.st_mode))
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
  int cpos = 0; /* Current character position in string */
  int c;
  int eolchar = '\n';

  /* Match tracking state */
  static char matches[MAX_MATCHES][TMPBUF_SIZE];
  int match_count = 0;
  int match_idx = -1;

  /* Clean initial buffer */
  memset (buf, 0, nbuf);

  /* Prompt the user for the input string */
  eprintf (prompt);

  for (;;)
    {
      /* Get a character from the user */
      c = ttgetc ();

      /* If it is a <ret>, change it to a <NL> */
      if (c == CCHR ('M'))
        c = '\n';

      /* Reset TAB match state whenever any non-completion key is pressed */
      if (c != 0x09 && c != ' ' && c != '?')
        {
          match_count = 0;
          match_idx = -1;
        }

      /* Line terminator: return completed string */
      if (c == eolchar)
        {
          buf[cpos] = '\0';

          /* Clear the message line */
          eerase ();
          ttflush ();

          if (buf[0] == '\0')
            return FALSE;

          return TRUE;
        }

      /* Abort input (^G) */
      if (c == CCHR ('G'))
        {
          ctrlg (FALSE, 0, KRANDOM);
          eputc (c);
          ttflush ();
          return ABORT;
        }

      /* Rubout / Erase (Backspace) */
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
              buf[cpos] = '\0';
              ttflush ();
            }
        }

      /* Kill line (^U) */
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
          buf[0] = '\0';
          ttflush ();
        }

      /* TAB or '?' — Filename completion */
      else if (c == 0x09 || c == ' ' || c == '?')
        {
          buf[cpos] = '\0';

          /* First TAB press: populate matches from directory */
          if (match_count == 0)
            {
              match_count = collect_matches (buf, matches, MAX_MATCHES);
              match_idx = 0;
            }
          else
            {
              /* Subsequent TAB press: cycle through matched files */
              match_idx = (match_idx + 1) % match_count;
            }

          if (match_count > 0)
            {
              int n;

              /* Erase current prompt line buffer from display */
              while (cpos > 0)
                {
                  outstring ("\b \b");
                  --ttcol;
                  cpos--;
                }

              /* Copy selected match into buffer */
              strncpy (buf, matches[match_idx], (size_t) (nbuf - 1));
              buf[nbuf - 1] = '\0';

              cpos = (int) strlen (buf);

              /* If single directory match, clear state so subsequent TAB enters directory */
              if (match_count == 1)
                {
                  if (cpos > 0 && buf[cpos - 1] == PATH_SEP)
                    {
                      match_count = 0;
                      match_idx = -1;
                    }
                }

              /* Render completed string back to editor terminal */
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
            ttbeep (); /* No matching files found */
        }

      /* Regular printable character input */
      else
        {
          if (cpos < nbuf - 1)
            {
              if ((c < ' ') && (c != '\n'))
                ttbeep ();
              else
                {
                  buf[cpos++] = c;
                  buf[cpos] = '\0';
                  ttputc (c);
                  ++ttcol;
                  ttflush ();
                }
            }
        }
    }
}

/*
 * Output a string of characters to terminal
 */
void
outstring (char *s)
{
  while (*s)
    ttputc (*s++);
}
