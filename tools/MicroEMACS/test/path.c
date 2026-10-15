#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../path.c"

int
main (void)
{
  char path[256];

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
    assert (strcmp (path_home (), "C:/Users/Home") == 0);
    assert (unsetenv ("HOME") == 0);

    assert (setenv ("USERPROFILE", "C:\\Users\\Profile", 1) == 0);
    assert (strcmp (path_home (), "C:/Users/Profile") == 0);
    assert (unsetenv ("USERPROFILE") == 0);

    assert (setenv ("HOMEDRIVE", "C:", 1) == 0);
    assert (setenv ("HOMEPATH", "\\Users\\Fallback", 1) == 0);
    assert (strcmp (path_home (), "C:/Users/Fallback") == 0);
    assert (unsetenv ("HOMEDRIVE") == 0);
    assert (unsetenv ("HOMEPATH") == 0);

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
