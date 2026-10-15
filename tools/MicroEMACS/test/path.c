#include <assert.h>
#include <stdio.h>
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
