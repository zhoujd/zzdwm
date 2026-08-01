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
static int is_regular_file (const char *path);
static int collect_matches (const char *prefix,
                            char matches[][TMPBUF_SIZE],
                            int max_matches,
                            int dir_only);

/*
 * Check if the given POSIX path points to a regular file (not a directory)
 */
static int
is_regular_file (const char *path)
{
  struct stat st;
  char native[TMPBUF_SIZE];

  if (path == NULL || *path == '\0')
    return 0;

  to_native_path (path, native, TMPBUF_SIZE);
  if (stat (native, &st) == 0)
    {
      if (S_ISREG (st.st_mode))
        return 1;
    }
  return 0;
}

/*
 * Converts backslashes to forward slashes, translates Windows drive
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
 * Simplifies path components by resolving '.' and '..' safely
 */
static void
normalize_path (char *path)
{
  char *src;
  char *dst;
  int root_len = 0;

  if (path == NULL || *path == '\0')
    return;

  /* Calculate and protect root prefix length */
  if (path[0] == '/' && path[1] == '/')
    {
      root_len = 2;
    }
  else if (path[0] == '/' && isalpha ((unsigned char)path[1]) &&
           (path[2] == '/' || path[2] == '\0'))
    {
      root_len = (path[2] == '/') ? 3 : 2;
    }
  else if (path[0] == '/')
    {
      root_len = 1;
    }

  src = path + root_len;
  dst = path + root_len;

  while (*src != '\0')
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

          /* Rewind dst safely */
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

      while (*src != '\0' && *src != '/')
        *dst++ = *src++;
    }

  /* Preserve trailing slash if original path ended with a separator */
  if (src > path && src[-1] == '/' && dst > path + root_len && dst[-1] != PATH_SEP)
    *dst++ = PATH_SEP;

  *dst = '\0';

  if (path[0] == '\0')
    strcpy (path, ".");
}

/*
 * Converts a POSIX MSYS-style path (/c/path) to a Windows native path (C:/path)
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
 * Perform in-memory directory scanning for filename/directory matching.
 */
static int
collect_matches (const char *prefix,
                 char matches[][TMPBUF_SIZE],
                 int max_matches,
                 int dir_only)
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

  strncpy (expanded_prefix, prefix, TMPBUF_SIZE - 1);
  expanded_prefix[TMPBUF_SIZE - 1] = '\0';

  sanitize_slashes (expanded_prefix);

  /* Handle ~ expansion using $HOME */
  if (expanded_prefix[0] == '~')
    {
      const char *home = getenv ("HOME");

#if defined (_WIN32) || defined (__MINGW32__)
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
#endif

      if (home != NULL && (expanded_prefix[1] == '/' || expanded_prefix[1] == '\0'))
        {
          char temp[TMPBUF_SIZE];
          size_t home_len = strlen (home);

          if (home_len < TMPBUF_SIZE)
            {
              memcpy (temp, home, home_len);
              strncpy (temp + home_len, expanded_prefix + 1, TMPBUF_SIZE - home_len - 1);
              temp[TMPBUF_SIZE - 1] = '\0';

              strncpy (expanded_prefix, temp, TMPBUF_SIZE - 1);
              expanded_prefix[TMPBUF_SIZE - 1] = '\0';

              sanitize_slashes (expanded_prefix);
            }
        }
    }

  normalize_path (expanded_prefix);

  strcpy (dirpath, ".");
  pattern[0] = '\0';

  /* Separate directory path from search pattern prefix */
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

  to_native_path (dirpath, native_dirpath, TMPBUF_SIZE);

  dir = opendir (native_dirpath);
  if (dir == NULL)
    return 0;

  pat_len = strlen (pattern);

  while ((entry = readdir (dir)) != NULL && match_count < max_matches)
    {
      if (strcmp (entry->d_name, ".") == 0
          || strcmp (entry->d_name, "..") == 0)
        continue;

#if defined (_WIN32) || defined (__MINGW32__)
      if (pat_len == 0 || _strnicmp (entry->d_name, pattern, pat_len) == 0)
#else
      if (pat_len == 0 || strncmp (entry->d_name, pattern, pat_len) == 0)
#endif
        {
          struct stat st;
          int ret;
          char full_check_path[TMPBUF_SIZE];
          char native_full_check[TMPBUF_SIZE];

          if (strcmp (dirpath, "/") == 0)
            ret = snprintf (full_check_path, TMPBUF_SIZE, "/%s", entry->d_name);
          else
            ret = snprintf (full_check_path, TMPBUF_SIZE, "%s/%s", dirpath, entry->d_name);

          if (ret < 0 || ret >= TMPBUF_SIZE)
            continue;

          to_native_path (full_check_path, native_full_check, TMPBUF_SIZE);

          int is_dir = (stat (native_full_check, &st) == 0 && S_ISDIR (st.st_mode));

          /* Filter out files if dir_only requested */
          if (dir_only && !is_dir)
            continue;

          /* Rebuild full matched display path cleanly */
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

          /* Append trailing slash for directories */
          if (is_dir)
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

  memset (buf, 0, nbuf);
  eprintf (prompt);

  for (;;)
    {
      c = ttgetc ();

      if (c == CCHR ('M'))
        c = '\n';

      /* Reset match state on non-completion key */
      if (c != 0x09 && c != ' ' && c != '?' && c != CCHR ('F') && c != 0x06)
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

      /* Ctrl+Q (0x11): quote */
      else if (c == CCHR ('Q') || c == 0x11)
        {
          if (cpos < nbuf - 1)
            {
              int q = ttgetc ();
              if (q != EOF)
                {
                  eputc (q);
                  buf[cpos++] = (char)q;
                  buf[cpos] = '\0';
                  ttflush ();
                }
            }
          else
            {
              ttbeep ();
            }
        }

      /* Ctrl+F (0x06): Advance into DIRECTORIES ONLY */
      else if (c == CCHR ('F') || c == 0x06)
        {
          buf[cpos] = '\0';

          /* FAST GUARD: If current buffer points directly to a file, beep and do nothing */
          if (is_regular_file (buf))
            {
              ttbeep ();
              continue;
            }

          match_count = collect_matches (buf, matches, MAX_MATCHES, 1);
          match_idx = 0;

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
              cpos = (int) strlen (buf);

              match_count = 0;
              match_idx = -1;

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
            {
              ttbeep ();
            }
        }

      /* Ctrl+B (0x02): Up to parent directory */
      else if (c == CCHR ('B') || c == 0x02)
        {
          buf[cpos] = '\0';

          while (cpos > 0)
            {
              outstring ("\b \b");
              --ttcol;
              cpos--;
            }

          if (buf[0] != '\0')
            {
              size_t len = strlen (buf);
              if (len > 1 && buf[len - 1] == PATH_SEP)
                buf[len - 1] = '\0';

              char *last_slash = strrchr (buf, PATH_SEP);
              if (last_slash != NULL)
                {
                  if (last_slash == buf)
                    {
                      buf[1] = '\0';
                    }
                  else
                    {
                      *last_slash = PATH_SEP;
                      *(last_slash + 1) = '\0';
                    }
                }
              else
                {
                  buf[0] = '\0';
                }
            }
          else
            {
              strcpy (buf, "../");
            }

          sanitize_slashes (buf);
          normalize_path (buf);

          match_count = 0;
          match_idx = -1;

          cpos = (int) strlen (buf);
          for (int n = 0; n < cpos; n++)
            {
              char ch = buf[n];
              if ((ch < ' ') && (ch != '\n'))
                {
                  outstring ("^");
                  ++ttcol;
                  ch ^= 0x40;
                }

              if (ch != '\n')
                ttputc (ch);
              else
                {
                  outstring ("<NL>");
                  ttcol += 3;
                }
              ++ttcol;
            }
          ttflush ();
        }

      /* Backspace */
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

      /* TAB / Space / '?' — Filename & Directory completion */
      else if (c == 0x09 || c == ' ' || c == '?')
        {
          buf[cpos] = '\0';

          if (match_count == 0)
            {
              match_count = collect_matches (buf, matches, MAX_MATCHES, 0);
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

              cpos = (int) strlen (buf);

              if (match_count == 1)
                {
                  if (cpos > 0 && buf[cpos - 1] == PATH_SEP)
                    {
                      match_count = 0;
                      match_idx = -1;
                    }
                }

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

      /* Character input */
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
