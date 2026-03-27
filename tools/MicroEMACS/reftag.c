/*
 * reftag.c
 */

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define NPAT   256
#define FALSE	0		/* False, no, bad, etc.         */
#define TRUE	1		/* True, yes, good, etc.        */

typedef int (*prepfunc)(const char *string);

typedef struct tagref
{
  char *string;			/* name of the tag              */
  int line;			/* line number                  */
  long offset;			/* file offset                  */
  int exact;			/* non-zero if exact match	*/
  struct tagref *next;		/* next tag in this file        */
  struct tagref *prev;		/* previous tag in this file    */
  struct tagfile *file;		/* file containing this tag     */
} tagref;

typedef struct tagfile
{
  char *fname;			/* name of file                 */
  struct tagfile *next;		/* next file on list            */
  struct tagfile *prev;		/* previous file on list        */
} tagfile;

/*
 * Global variables
 */
tagfile tagfilelist = {		/* list of tag files            */
  "",				/* dummy filename               */
  &tagfilelist,			/* head pointer                 */
  &tagfilelist			/* tail pointer                 */
};

tagref tagreflist = {		/* list of all tags             */
  "",				/* dummy tag string             */
  0,				/* line number                  */
  0,				/* offset                       */
  0,				/* exact			*/
  &tagreflist,			/* head pointer                 */
  &tagreflist,			/* tail pointer                 */
  NULL				/* file pointer                 */
};

static FILE *ffp;		/* text, profile, and journal files     */
/*
 * Buffer for ffgetline.  Dynamically allocated to handle any line length.
 */
static char *buf;		/* dynamic line buffer */
static int bufsize;		/* size of line buffer */

/*
 * File I/O.
 */
#define FIOSUC	0		/* Success.                     */
#define FIOFNF	1		/* File not found.              */
#define FIOEOF	2		/* End of file.                 */
#define FIOERR	3		/* Error.                       */

/*
 * Open a file for reading.
 */
int
ffropen (const char *fn)
{
  if ((ffp = fopen (fn, "r")) == NULL)
    return (FIOFNF);
  return (FIOSUC);
}

/*
 * Read a line from a file, and store the bytes
 * in a local buffer. Stop on end of file or end of
 * line. Don't get upset by files that don't have an end of
 * line on the last line; this seem to be common on CP/M-86 and
 * MS-DOS (the suspected culprit is VAX/VMS kermit, but this
 * has not been confirmed. If this is sufficiently researched
 * it may be possible to pull this kludge). Delete any CR
 * followed by an LF. This is mainly for runoff documents,
 * both on VMS and on Ultrix (they get copied over from
 * VMS systems with DECnet).  Return the address of the local
 * buffer to *buf, and the number of bytes read to *nbytes.
 */
int
ffgetline (char **bufp, int *nbytes)
{
  register int c;
  register int i;
  char *newbuf;
  int newbufsize;
  int merror;

  i = 0;
  merror = 0;
  for (;;)
    {
      c = getc (ffp);

      /*  Delete carriage return if followed by line feed
       */
      if (c == '\r')
        {			/* carriage return?     */
          c = getc (ffp);	/* check next byte      */
          if (c != '\n')
            {			/* next not line feed?  */
              ungetc (c, ffp);	/* put it back          */
              c = '\r';		/* put cr into line     */
            }
        }

      if (c == EOF || c == '\n')	/* end of line/file?    */
        break;

      /*  If the buffer is too small, enlarge it.
       */
      if (i >= bufsize)
        {			/* line full?           */
          newbufsize = bufsize + 256;
          if (bufsize != 0)
            newbuf = realloc (buf, newbufsize);
          else
            newbuf = malloc (newbufsize);
          if (!newbuf)
            {
              ungetc (c, ffp);	/* put char back        */
              printf ("Out of memory, line split\n");
              merror = 1;
              break;
            }
          buf = newbuf;
          bufsize = newbufsize;
        }
      buf[i++] = c;		/* store byte in line   */
    }
  *nbytes = i;			/* return no. of bytes  */
  *bufp = buf;			/* return buf pointer   */
  if (c != '\n')
    {				/* End of file.         */
      if (merror)		/* out of memory?       */
        return (FIOERR);	/* report error         */
      else
        return (FIOEOF);	/* normal end of file   */
    }
  return (FIOSUC);
}

/*
 * Close a file.
 * Should look at the status.
 */
int
ffclose (void)
{
  fclose (ffp);
  return (FIOSUC);
}

/*
 * Add a filename to the end of the filename list, return a
 * pointer to the next file structure, or NULL if out of memory.
 */
tagfile *
addtagfile (const char *name)
{
  tagfile *newfile;
  int len = strlen (name) + 1;

  if ((newfile = malloc (sizeof (tagfile))) == NULL)
    return NULL;

  if ((newfile->fname = malloc (len)) == NULL)
    return NULL;
  strcpy (newfile->fname, name);

  /* Add this file to the end of the file list.
   */
  newfile->next = &tagfilelist;
  newfile->prev = tagfilelist.prev;
  tagfilelist.prev = newfile->prev->next = newfile;

  return newfile;
}

/*
 * Search the filename list for the specified filename, and add it
 * to the list if not found.  Return a pointer to the filename structure,
 * or NULL if out of memory.
 */
tagfile *
findtagfile (const char *name)
{
  tagfile *f;

  for (f = tagfilelist.next; f != &tagfilelist; f = f->next)
    if (strcmp (name, f->fname) == 0)
      return f;
  return addtagfile (name);
}

/*
 * Add a new tag reference to the list, return a pointer to the
 * newly allocated tag structure.
 */
tagref *
addtagref (const char *string, tagfile *file, int line, long offset,
           int exact)
{
  int len = strlen (string) + 1;
  tagref *newref, *prev, *next;

  /* Make sure there aren't any duplicates in the list.
   */
  for (next = tagreflist.next;
       next != &tagreflist;
       next = next->next)
    {
      if (file == next->file &&
          line == next->line)
        {
          return next;
        }
    }

  /* Allocate space for the tag reference and
   * the string, and make a copy of the string.
   */
  if ((newref = malloc (sizeof (tagref))) == NULL)
    return NULL;
  if ((newref->string = malloc (len + 1)) == NULL)
    {
      free (newref);
      return NULL;
    }
  strcpy (newref->string, string);

  /* Fill in the rest of the fields.
   */
  newref->line   = line;
  newref->offset = offset;
  newref->file   = file;
  newref->exact  = exact;

  /* If exact is true, add it to the beginning of the list after any other
   * exact matches; otherwise append to the tail of the list.
   */
  if (exact)
    {
      for (prev = &tagreflist, next = tagreflist.next;
           next->exact != 0;
           prev = next, next = next->next)
        ;
      newref->next = next;
      newref->prev = prev;
      prev->next = next->prev = newref;
    }
  else
    {
      newref->next = &tagreflist;
      newref->prev = tagreflist.prev;
      tagreflist.prev = newref->prev->next = newref;
    }

  return newref;
}


/*
 * Read a tag file, parse into our internal format,
 * store in taglist.  Return false if error,
 * or true if successful.
 */
int
readtagfile (char *fname)
{
  int		s;
  char *	line;
  int		nbytes;
  int		istagline;
  char *	eol;
  tagfile *	curfile = NULL;
  tagref *	newref;
  char *	tmp;
  int		lineno;
  long		offset;

  /* Open the TAGS file.
   */
  if ((s = ffropen (fname)) != FIOSUC)
    {				/* can't open tag file? */
      printf ("Unable to open tag file %s\n", fname);
      return FALSE;
    }

  /* Parse the tags file, store it in internal format, in
   * lists of files and references.
   */
  istagline = TRUE;
  do
    {
      s = ffgetline (&line, &nbytes);	/* read next line       */
      if (s != FIOSUC && nbytes == 0)	/* True end-of-file?    */
        break;
      line[nbytes] = '\0';	/* terminate the line   */

      /* A line consisting of a form feed indicates
       * the start of the tags for a new file.  The following
       * line will give the filename.
       */
      if (line[0] == '\f')
        {			/* form feed?           */
          istagline = FALSE;	/* next line is filename */
          continue;
        }
      if (istagline)
        {			/* this is a tag line   */

          /* There must have been a previous file line.
           */
          if (curfile == NULL)
            {
              printf ("Reference without file\n");
              break;
            }

          /* The tag string is terminated by a rubout.
           */
          if ((eol = strchr (line, 0x7f)) == NULL)
            {
              printf ("Tag line missing 0x7f\n");
              s = FIOERR;
              break;
            }
          *eol++ = '\0';	/* skip over rubout, terminate string */

          /* The tag string is followed by the line number
           * and file offset.  However, definitions of macros
           * that take arguments are followed by the non-
           * parenthesized macro name terminated by a Control-A.
           * For now, ignore this and skip past the Control-A.
           */
          if ((tmp = strchr (eol, '\001')) != NULL)
            eol = tmp + 1;	/* skip past control-A */
          lineno = strtol (eol, &eol, 10);
          if (lineno == 0 || *eol != ',')
            {
              printf ("Badly formed line number for %s\n", line);
              /* s = FIOERR; */
              /* break; */
              continue;
            }
          eol++;		/* skip over comma */
          offset = strtol (eol, NULL, 10);

          /* Add this to the end of the list of references.
           */
          if ((newref = addtagref (line, curfile, lineno, offset, 0)) == NULL)
            {
              printf ("Unable to allocate tagref\n");
              s = FIOERR;
              break;
            }
        }
      else
        {
          /* This is a file line, not a tag line, so must create new
           * file record. Filename is followed by a comma and the
           * number of characters to skip to get
           * to the next control-L.  Make a copy of the
           * filename but ignore the number of characters.
           */
          if ((eol = strrchr (line, ',')) == NULL)
            {
              printf ("Filename not followed by comma\n");
              s = FIOERR;
              break;
            }
          *eol = '\0';		/* null-terminate the filename */

          if ((curfile = addtagfile (line)) == NULL)
            {
              printf ("Unable to allocate file struct\n");
              s = FIOERR;
              break;
            }

          istagline = TRUE;	/* next line is a tag line */
        }

    }
  while (s == FIOSUC);		/* until error or EOF   */
  ffclose ();			/* Ignore errors.       */
  return (s != FIOERR);
}


/*
 * Read the tags file if the tag list is empty.
 */
int
preptag (const char *string)
{
  return readtagfile ("TAGS");
}

/*
 * Prompt the user for a tag string to search for, and call the
 * caller-specified 'prep' function to prime the tag list for
 * an initial search.  Then search the tag list for the string,
 * and if found, read in the file given in the tag, and position
 * the cursor at the tag's line number.  If an argument is
 * given (f is TRUE), search for the next occurrence of the most recent tag
 * string specified.  If the argument (n) is negative, search
 * for the previous occurrence of the tag string.
 */
int
searchtag (prepfunc prep, const char *tagpat)
{
  tagref *r;			/* current tag          */
  int bfound = FALSE;

  /* Prepare things for an initial search.  This gives the
   * caller an opportunity to prime the tag list based on the
   * string specified by the user.
   */
  if (prep (tagpat) == FALSE)
    return FALSE;
  /* Start searching at the top of the tag list.
   */
  r = tagreflist.next;

  /* Search the tags list for the specified tag.
   */
  while (r != &tagreflist)
    {
      if (strstr (r->string, tagpat) != NULL)
      {
        printf ("Ref %s, file %s, line %ld, offset %ld\n",
                r->string, r->file->fname, r->line, r->offset);
        bfound = TRUE;
      }
      r = r->next;	/* skip to next tag */
    }
  if (r == &tagreflist && bfound == FALSE)
    {
      printf ("No tags for %s\n", tagpat);
      return FALSE;
    }
  return TRUE;
}

/*
 * main function
 */
int
main (int argc, char* argv[])
{
  if (argc != 2)
   {
     printf ("Usage: reftag {PATTERN}\n"
             "Example:\n"
             "  reftag main\n");
     return FALSE;
   }
  return searchtag (preptag, argv[1]);
}
