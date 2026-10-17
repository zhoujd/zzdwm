#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef MINGW
int setenv (const char *name, const char *value, int overwrite);
int unsetenv (const char *name);
#endif

#include "../path.c"

#ifdef MINGW
void eprintf (const char *format, ...);
int dosmode;
BUFFER *curbp;
int zflag;

void
eprintf (const char *format, ...)
{
  (void) format;
}

int
setenv (const char *name, const char *value, int overwrite)
{
  char *combined;
  size_t size;

  (void) overwrite;
  size = strlen (name) + strlen (value) + 2;
  combined = malloc (size);
  if (combined == NULL)
    return -1;
  snprintf (combined, size, "%s=%s", name, value);
  putenv (combined);
  return 0;
}

int
unsetenv (const char *name)
{
  char *empty;
  size_t size = strlen (name) + 2;

  empty = malloc (size);
  if (empty == NULL)
    return -1;
  snprintf (empty, size, "%s=", name);
  putenv (empty);
  free (empty);
  return 0;
}
#endif

#ifdef MINGW
#include "../sys/mingw/fileio.c"
#endif

int
main (void)
{
  char path[256];

#ifdef MINGW
  char name[256] = "/tmp/microemacs-path-test/";
  char selected[256];
  char *entry;
#endif

  assert (path_is_separator ('/') == TRUE);
  assert (path_is_absolute ("/tmp") == TRUE);
  assert (path_is_absolute ("tmp") == FALSE);
  assert (path_has_separator ("tmp/file") == TRUE);
  assert (path_has_separator ("tmp") == FALSE);

  {
    const char *home = getenv ("HOME");

    if (home != NULL)
      assert (strcmp (path_home (), home) == 0);
  }

#ifdef MINGW
  {
    const char *home = getenv ("HOME");
    const char *userprofile = getenv ("USERPROFILE");
    const char *homedrive = getenv ("HOMEDRIVE");
    const char *homepath = getenv ("HOMEPATH");

    assert (unsetenv ("HOME") == 0);
    assert (unsetenv ("USERPROFILE") == 0);
    assert (unsetenv ("HOMEDRIVE") == 0);
    assert (unsetenv ("HOMEPATH") == 0);
    assert (path_home () == NULL);

    assert (setenv ("HOME", "/c/Users/Home", 1) == 0);
    assert (strcmp (path_home (), "/c/Users/Home") == 0);
    assert (unsetenv ("HOME") == 0);

    assert (setenv ("USERPROFILE", "C:\\Users\\Profile", 1) == 0);
    assert (strcmp (path_home (), "/c/Users/Profile") == 0);
    assert (unsetenv ("USERPROFILE") == 0);

    assert (setenv ("HOMEDRIVE", "C:", 1) == 0);
    assert (setenv ("HOMEPATH", "\\Users\\Fallback", 1) == 0);
    assert (strcmp (path_home (), "/c/Users/Fallback") == 0);
    assert (unsetenv ("HOMEDRIVE") == 0);
    assert (unsetenv ("HOMEPATH") == 0);

    path_to_native ("/c/Windows/System32", path, sizeof (path));
    assert (strcmp (path, "C:/Windows/System32") == 0);

    path_to_native ("/c", path, sizeof (path));
    assert (strcmp (path, "C:/") == 0);

    if (home != NULL)
      assert (setenv ("HOME", home, 1) == 0);
    if (userprofile != NULL)
      assert (setenv ("USERPROFILE", userprofile, 1) == 0);
    if (homedrive != NULL)
      assert (setenv ("HOMEDRIVE", homedrive, 1) == 0);
    if (homepath != NULL)
      assert (setenv ("HOMEPATH", homepath, 1) == 0);
  }
#endif

#ifdef MINGW
  assert (mkdir (name) == 0 || errno == EEXIST);
  snprintf (selected, sizeof (selected), "%sselected", name);
  assert (mkdir (selected) == 0 || errno == EEXIST);

  assert (setenv ("HOME", name, 1) == 0);
  entry = ffsearch ("~/", 2, NULL, TRUE);
  assert (entry != NULL);
  assert (strncmp (entry, "~/", 2) == 0 && entry[2] != 0);
  assert (ffsearch ("~/", 2, entry, TRUE) == NULL);

  entry = ffsearch ("./", 2, NULL, TRUE);
  assert (entry != NULL);
  assert (strncmp (entry, "./", 2) == 0 && entry[2] != 0);
  assert (ffsearch ("./", 2, entry, TRUE) == NULL);

  entry = ffsearch (name, strlen (name), NULL, TRUE);
  assert (entry != NULL);
  assert (ffsearch (name, strlen (name), entry, TRUE) == NULL);
#endif

  strcpy (path, "/tmp/./microemacs/../me/");
  path_normalize (path);
  assert (strcmp (path, "/tmp/me/") == 0);

  strcpy (path, "../src/./file.c");
  path_normalize (path);
  assert (strcmp (path, "../src/file.c") == 0);

  path_to_native ("/tmp/microemacs/../me", path, sizeof (path));
  assert (strcmp (path, "/tmp/me") == 0);

  path_to_posix ("C:/Windows/System32", path, sizeof (path));
  assert (strcmp (path, "/c/Windows/System32") == 0);

  path_to_posix ("C:\\Windows\\System32", path, sizeof (path));
  assert (strcmp (path, "/c/Windows/System32") == 0);

  assert (path_join ("/tmp/me", "file.c",
                     path, sizeof (path)) == TRUE);
  assert (strcmp (path, "/tmp/me/file.c") == 0);
  assert (path_join ("/tmp/me/", "file.c",
                     path, sizeof (path)) == TRUE);
  assert (strcmp (path, "/tmp/me/file.c") == 0);

  assert (path_join ("/tmp/me", "file.c", path, 8) == FALSE);

  printf ("path tests passed\n");
  return 0;
}
