/*
    Copyright (C) 2008 Mark Alexander

    This file is part of MicroEMACS, a small text editor.

    MicroEMACS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "def.h"

/* Uncomment this line to build test program. */
/* #define TEST 1 */

/*
 * Local variables.
 */
static FILE *cscope_input;
static FILE *cscope_output;

/*
 * Ignore the prompt characters from cscope.
 */
static void
ignore_prompt (void)
{
  if (fgetc (cscope_input) != '>' || fgetc (cscope_input) != '>' ||
      fgetc (cscope_input) != ' ')
    {
#if TEST
      printf ("bad prompt from cscope!\n");
#endif
    }
}

/*
 * Initiate a search to cscope, return the number of matches.  The
 * 'searchfield' parameter says which field of of the cscope entry screen
 * on which to enter the string ('0' is "Find this C symbol", '6' is
 * "Find this egrep pattern", etc.).
 */
static int
cscope_search (char search_field, const char *search_string)
{
  int nlines, result;
  char buf[64];

  /* Ignore the ">> " prompt, which should be waiting for us already. */
  ignore_prompt ();

  /* Issue the command Nsearchstring, where N is the field number.
   */
  fputc (search_field, cscope_output);
  fputs (search_string, cscope_output);
  fputc ('\n', cscope_output);
  fflush (cscope_output);
#if TEST
  printf ("sent command '0%s'\n", search_string);
#endif

  /* Read the first line, which is of the format 'cscope: n lines',
     and parse the 'n'. */
  if (fgets (buf, sizeof (buf), cscope_input) == NULL)
    return 0;
#if TEST
  printf ("result of query: '%s'\n", buf);
#endif
  result = sscanf (buf, "cscope: %d lines", &nlines);
  return result == 1 ? nlines : 0;
}

/*
 * Read the next match from the previously initiated search.
 * Return the file to 'filename', the name of the function containing
 * the string to 'where', and the line number to 'line_number'.
 * FIXME: we should do length-checking on the buffers.
 */
static void
next_match (char *filename, char *where, int *line_number)
{
  char buf[1024];
#if TEST
  int ret;
#endif

  if (fgets (buf, sizeof (buf), cscope_input) == NULL)
    return;
#if TEST
  printf ("read line: '%s'\n", buf);
  ret =
#endif
  sscanf (buf, "%s %s %d", filename, where, line_number);
#if TEST
  printf ("sscanf returned %d\n", ret);
#endif
}

/*
 * Open a two-way pipe to the cscope program.
 *
 * If noupdatecscope is TRUE, pass the -d option so that cscope will not
 * try to update to update the cross reference.  This is useful
 * when using cscope databases generated by other programs, such
 * as starscope (Ruby).
 */
static int
open_cscope (void)
{
  const char *args[5];

  args[0] = "cscope";
  args[1] = "-l";
  args[2] = "-k";
  if (noupdatecscope)
    {
      args[3] = "-d";
      args[4] = NULL;
    }
  else
    args[3] = NULL;
  return openpipe ("cscope", args, &cscope_input, &cscope_output);
}

/*
 * Issue a cscope search in the specified entry field for the string,
 * read back the resulting matches, and store them in the tags list.
 */
static int
prepcscope (char field, const char *string, int delete)
{
  int		n;
  char		filename[1024];
  char		where[1024];
  int		line;
  tagfile *	f;
  int		exact;

  /* Open a pipe to cscope if not already done.
   */
  if (cscope_input == NULL)
    if (open_cscope () == FALSE)
      {
	eprintf ("Unable to open a pipe to cscope");
	return FALSE;
      }

  /* If delete flag is TRUE, free up any existing tags from a previous search.
   */
  if (delete)
    freetags (FALSE, 1, KRANDOM);

  /* Initiate a search to cscope, get back the number of matches.
   */
  n = cscope_search (field, string);

  /* Add each of the matches to the tag list.
   */
  while (n-- > 0)
    {
      /* Get the result of the next match.
       */
      next_match (filename, where, &line);

      /* Create a file entry for this file if not already in the list.
       */
      f = findtagfile (filename);
      if (f == NULL)
	{
	  eprintf ("Unable to create file structure");
	  return FALSE;
	}

      /* If the search string is the same as the name of the function where this
       * reference was found, this must be the definition of the function,
       * so put the tag at the head of the list instead of the end.
       */
      exact = strcmp (where, string) == 0;

      /* Add a tag entry to the list.
       */
      if (addtagref (string, f, line, 0L, exact) == NULL)
	{
	  eprintf ("Unable to create tag structure");
	  return FALSE;
	}
    }

  return TRUE;
}

/*
 * Prepare for scanning through the tags for the given C symbol.
 * This function is a callback called by searchtag (in tags.c)
 * just before it starts searching through the tag list.
 */
static int
prepref (const char *string)
{
  return prepcscope ('1', string, TRUE) && prepcscope ('0', string, FALSE);
}

/*
 * Search for a cscope reference.  All of the work involved in searching
 * the tag list is done in searchtag (tags.c), but the actual creation
 * of the tag list is done in prepref above.
 */
int
findcscope (int f, int n, int k)
{
  return searchtag (f, n, prepref, "ref");
}

/*
 * Search for the next cscope reference.
 */
int
nextcscope (int f, int n, int k)
{
  return searchtag (1, n, prepref, "ref");
}

/*
 * Prepare for scanning through the egrep searches for the given string.
 * This function is a callback called by searchtag (in tags.c)
 * just before it starts searching through the tag list.
 */
static int
prepgrep (const char *string)
{
  return prepcscope ('6', string, TRUE);
}

/*
 * Search for an egrep reference.  All of the work involved in searching
 * the tag list is done in searchtag (tags.c), but the actual creation
 * of the tag list is done in prepgrep above.
 */
int
findgrep (int f, int n, int k)
{
  return searchtag (f, n, prepgrep, "grep");
}

/*
 * test program
 */
#if TEST
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
      int n = cscope_search (search_string);
      printf ("%d matches for %s:\n", n, search_string);
      while (n-- > 0)
	{
	  next_match (filename, where, &line);
	  printf ("%s:%d in %s\n", filename, line, where);
	}
    }
  return 0;
}
#endif
