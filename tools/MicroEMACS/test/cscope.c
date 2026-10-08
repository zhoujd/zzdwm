#include <stdio.h>

#include "../cscope.c"

char *cscope_path = "cscope";
int noupdatecscope;

int
main (int argc, char *argv[])
{
  int i;
  char filename[1024];
  char where[1024];
  int line;

  if (open_cscope () == FALSE)
    {
      printf ("unable to open pipe to cscope\n");
      return 1;
    }

  for (i = 1; i < argc; i++)
    {
      const char *search_string = argv[i];
      int n = cscope_search ('0', search_string);
      printf ("%d matches for %s:\n", n, search_string);
      while (n-- > 0)
        {
          next_match (filename, where, &line);
          printf ("%s:%d in %s\n", filename, line, where);
        }
    }
  return 0;
}
