#include <stdio.h>
#include <assert.h>
#include <string.h>

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

  assert (parse_match ("sample.c sample_function 3",
                       filename, sizeof (filename),
                       where, sizeof (where), &line) == TRUE);
  assert (strcmp (filename, "sample.c") == 0);
  assert (strcmp (where, "sample_function") == 0);
  assert (line == 3);

  assert (parse_match ("sample.c <global> 1",
                       filename, sizeof (filename),
                       where, sizeof (where), &line) == TRUE);
  assert (strcmp (where, "<global>") == 0);

  assert (parse_match ("sample.c sample_function bad",
                       filename, sizeof (filename),
                       where, sizeof (where), &line) == FALSE);
  assert (parse_match ("sample.c",
                       filename, sizeof (filename),
                       where, sizeof (where), &line) == FALSE);
  assert (parse_match ("toolong name 1",
                       filename, 5, where, sizeof (where), &line) == FALSE);

  if (open_cscope () == FALSE)
    {
      printf ("unable to open pipe to cscope\n");
      return 1;
    }

  for (i = 1; i < argc; i++)
    {
      const char *search_string = argv[i];
      int n = cscope_search ('0', search_string);
      if (n < 0)
        {
          printf ("cscope search failed for %s\n", search_string);
          return 1;
        }
      printf ("%d matches for %s:\n", n, search_string);
      while (n-- > 0)
        {
          if (next_match (filename, sizeof (filename),
                          where, sizeof (where), &line) == FALSE)
            {
              printf ("malformed cscope response for %s\n", search_string);
              return 1;
            }
          printf ("%s:%d in %s\n", filename, line, where);
        }
    }
  return 0;
}
