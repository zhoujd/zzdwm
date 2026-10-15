/*
 * Shared path helpers.
 */

#include "def.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef MINGW
#ifdef __MINGW32__
#include <direct.h>
#else
char *_fullpath (char *result, const char *path, size_t result_size);
#endif
#else
#include <unistd.h>
#endif

int
path_is_separator (char character)
{
#ifdef MINGW
  return character == '/' || character == '\\';
#else
  return character == '/';
#endif
}

int
path_is_absolute (const char *path)
{
  if (path == NULL || path[0] == 0)
    return FALSE;
  if (path_is_separator (path[0]))
    return TRUE;
#ifdef MINGW
  if (isalpha ((unsigned char) path[0]) && path[1] == ':')
    return TRUE;
#endif
  return FALSE;
}

int
path_has_separator (const char *path)
{
  const char *position;

  if (path == NULL)
    return FALSE;
  for (position = path; *position != 0; position++)
    if (path_is_separator (*position))
      return TRUE;
  return FALSE;
}

const char *
path_home (void)
{
#ifdef MINGW
  static char windows_home[NLINE * 2];
  const char *home;
  const char *drive;
  const char *path;
  int length;

  home = getenv ("HOME");
  if (home == NULL)
    home = getenv ("USERPROFILE");
  if (home != NULL)
    {
      path_to_native (home, windows_home, sizeof (windows_home));
      if (windows_home[0] == 0)
        return NULL;
      return windows_home;
    }

  drive = getenv ("HOMEDRIVE");
  path = getenv ("HOMEPATH");
  if (drive == NULL || path == NULL)
    return NULL;
  length = snprintf (windows_home, sizeof (windows_home), "%s%s", drive, path);
  if (length < 0 || (size_t) length >= sizeof (windows_home))
    return NULL;
  path_normalize (windows_home);
  return windows_home;
#else
  return getenv ("HOME");
#endif
}

static int
path_root_length (const char *path)
{
  if (path_is_separator (path[0]))
    {
      if (path_is_separator (path[1]))
        return 2;
#ifdef MINGW
      if (isalpha ((unsigned char) path[1])
          && (path_is_separator (path[2]) || path[2] == 0))
        return path_is_separator (path[2]) ? 3 : 2;
#endif
      return 1;
    }
#ifdef MINGW
  if (isalpha ((unsigned char) path[0]) && path[1] == ':')
    return path_is_separator (path[2]) ? 3 : 2;
#endif
  return 0;
}

void
path_normalize (char *path)
{
  char *source;
  char *destination;
  int root_length;

  if (path == NULL || path[0] == 0)
    return;

#ifdef MINGW
  for (source = path; *source != 0; source++)
    if (*source == '\\')
      *source = '/';
#endif

  root_length = path_root_length (path);
  source = path + root_length;
  destination = path + root_length;

  while (*source != 0)
    {
      if (path_is_separator (*source))
        {
          source++;
          continue;
        }
      if (source[0] == '.'
          && (source[1] == 0 || path_is_separator (source[1])))
        {
          source += source[1] == 0 ? 1 : 2;
          continue;
        }
      if (source[0] == '.' && source[1] == '.'
          && (source[2] == 0 || path_is_separator (source[2])))
        {
          source += source[2] == 0 ? 2 : 3;
          if (destination > path + root_length)
            {
              while (destination > path + root_length
                     && destination[-1] != '/')
                destination--;
              if (destination > path + root_length
                  && destination[-1] == '/')
                destination--;
            }
          else if (root_length == 0)
            {
              if (destination > path && destination[-1] != '/')
                *destination++ = '/';
              *destination++ = '.';
              *destination++ = '.';
            }
          continue;
        }
      if (destination > path && destination[-1] != '/'
          && (root_length == 0 || destination > path + root_length
              || path[root_length - 1] == '/'))
        *destination++ = '/';
      while (*source != 0 && !path_is_separator (*source))
        *destination++ = *source++;
    }

  if (source > path && path_is_separator (source[-1])
      && destination > path + root_length && destination[-1] != '/')
    *destination++ = '/';
  if (destination == path)
    *destination++ = '.';
  *destination = 0;
}

void
path_to_native (const char *path, char *result, size_t result_size)
{
  int length;

  if (result == NULL || result_size == 0)
    return;
  if (path == NULL)
    {
      result[0] = 0;
      return;
    }

#ifdef MINGW
  if (path[0] == '/' && isalpha ((unsigned char) path[1])
      && (path[2] == '/' || path[2] == 0))
    {
      length = snprintf (result, result_size, "%c:/%s",
                         toupper ((unsigned char) path[1]), path + 3);
      if (length < 0 || (size_t) length >= result_size)
        {
          result[0] = 0;
          return;
        }
      path_normalize (result);
      return;
    }
#endif

  length = snprintf (result, result_size, "%s", path);
  if (length < 0 || (size_t) length >= result_size)
    {
      result[0] = 0;
      return;
    }
  path_normalize (result);
}

void
path_to_posix (const char *path, char *result, size_t result_size)
{
  char *position;
  int length;

  if (result == NULL || result_size == 0)
    return;
  if (path == NULL)
    {
      result[0] = 0;
      return;
    }

  if (path[0] != 0 && path[1] == ':')
    length = snprintf (result, result_size, "/%c/%s",
                       tolower ((unsigned char) path[0]), path + 2);
  else
    length = snprintf (result, result_size, "%s", path);
  if (length < 0 || (size_t) length >= result_size)
    {
      result[0] = 0;
      return;
    }
  for (position = result; *position != 0; position++)
    if (*position == '\\')
      *position = '/';
  path_normalize (result);
}

int
path_join (const char *directory, const char *component,
           char *result, size_t result_size)
{
  size_t directory_length;
  const char *separator;
  int length;

  if (directory == NULL || directory[0] == 0
      || component == NULL || component[0] == 0
      || result == NULL || result_size == 0)
    return FALSE;

  directory_length = strlen (directory);
  separator = path_is_separator (directory[directory_length - 1]) ? "" : "/";
  length = snprintf (result, result_size, "%s%s%s",
                     directory, separator, component);
  if (length < 0 || (size_t) length >= result_size)
    return FALSE;
  path_normalize (result);
  return TRUE;
}

int
path_canonical_directory (const char *directory,
                          char *result, size_t result_size)
{
  if (directory == NULL || result == NULL || result_size == 0)
    return FALSE;

#ifdef MINGW
  char *native;
  int status;

  native = malloc (result_size);
  if (native == NULL)
    return FALSE;
  path_to_native (directory, native, result_size);
  if (native[0] == 0)
    {
      free (native);
      return FALSE;
    }
  status = _fullpath (result, native, result_size) != NULL;
  free (native);
  if (status == FALSE)
    return FALSE;
  path_normalize (result);
  return TRUE;
#else
  char *canonical;
  int length;

  if (directory == NULL || result == NULL || result_size == 0)
    return FALSE;
  canonical = realpath (directory, NULL);
  if (canonical == NULL)
    return FALSE;
  length = snprintf (result, result_size, "%s", canonical);
  free (canonical);
  return length >= 0 && (size_t) length < result_size;
#endif
}
