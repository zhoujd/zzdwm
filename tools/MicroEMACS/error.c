/* error.c */

#include	"def.h"
#include	<string.h>
#include	<unistd.h>
 
/*
 * Visit the file and line mentioned in the next gcc error message in the
 * current buffer.
 */
int
gccerror (int f, int n, int k)
{
  LINE *lp;
  BUFFER *bp;
  EWINDOW *wp;
  static const char *fmt = "%" STRINGIFY(NFILEN) "[^:]:%d:%d: %n";
  static const char *pfx = "In file included from";
  int pfxlen;
  char filename[NFILEN];
  int line, column;
  uchar *str;
  int len, chars, wlen;
  uchar *copy = NULL;

  lp = curwp->w_dot.p;		/* Cursor location.	*/
  if (curwp->w_dot.o != 0)	/* Skip to next line	*/
    lp = lforw (lp);		/*  if not at column 1	*/
  bp = curwp->w_bufp;
  pfxlen = strlen(pfx);
  while (lp != bp->b_linep)
    {
      /* Make a copy of the line with a null termination
       * so that sscanf won't run off the end.
       */
      str = lgets (lp);
      len = llength (lp);
      copy = realloc (copy, len + 1);
      memcpy (copy, str, len);
      copy[len] = '\0';

      /* Test that the line doesn't start with a non-error prefix,
       * and that it starts with the pattern filename:line:column: .
       */
      if (strncmp ((const char *) copy, pfx, pfxlen) != 0 &&
          sscanf ((const char *) copy, fmt, filename, &line, &column, &chars) == 3)
        {
          /* Move cursor past the filename:line:column.
           */
          curwp->w_dot.p = lp;
          curwp->w_dot.o = unslen (str, chars);
          curwp->w_flag |= WFMOVE;

          /* Check if file exists.
           */
          if (access (filename, R_OK) != F_OK)
            {
              eprintf ("Cannot read '%s'", filename);
              free (copy);
              return FALSE;
            }

          /* Pop up a window and read the indicated file into it.
           */
          if ((wp = wpopup ()) == NULL)
            {
              free (copy);
              return FALSE;
            }
          curwp = wp;
          if (visit_file (filename) == FALSE)
            {
              free (copy);
              return FALSE;
            }

          /* Move to the indicated line and column.
           */
          if (gotoline (TRUE, line, 0) == FALSE)
            {
              free (copy);
              return FALSE;
            }
          wlen = wllength (curwp->w_dot.p);
          if (column >= wlen)
            curwp->w_dot.o = wlen;
          else
            curwp->w_dot.o = column - 1;

          /* Put as much of the error message as will fit
           * on the echo line.
           */
          len = len - chars;
          str = copy + chars;
          if (unslen (str, len) > ncol)
            {
              len = uoffset (str, ncol);
              str[len] = '\0';
            }
          eprintf ("%s", str);
          free (copy);
          return TRUE;
        }
      lp = lforw (lp);
    }
  eprintf ("gcc error not found");
  if (copy)
    free (copy);
  return FALSE;
}
