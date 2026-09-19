/*
 * Filename error.c
 */
#include "def.h"
#include <string.h>
#include <unistd.h>

/*
 * Find a window associated with the buffer list.
 */
EWINDOW *wfind(BUFFER *bp)
{
  register EWINDOW *wp;

  wp = wheadp;
  while (wp != NULL)
    {
      if (wp->w_bufp == bp)
        return wp;
      wp = wp->w_wndp;
    }
  return NULL;
}

/*
 * Unified error / symbol navigation parser for MicroEMACS
 */
int
gotoerror (int f, int n, int k)
{
  LINE *lp;
  BUFFER *bp;
  EWINDOW *wp;
  static const char *pfx = "In file included from";
  int pfxlen;
  char filename[NFILEN];
  int line = 0, column = 0;
  uchar *str;
  int len, chars = 0, wlen;
  char *copy = NULL;

  lp = curwp->w_dot.p;         /* Start at current buffer line */
  if (curwp->w_dot.o != 0)     /* If cursor is mid-line, advance to next line */
    lp = lforw (lp);

  bp = curwp->w_bufp;
  pfxlen = strlen (pfx);

  while (lp != bp->b_linep)
    {
      str = lgets (lp);
      len = llength (lp);
      copy = realloc (copy, len + 1);
      if (copy == NULL)
        {
          eprintf ("Out of memory");
          return FALSE;
        }
      memcpy (copy, str, len);
      copy[len] = '\0';

      /* Skip header lines like "In file included from..." or empty lines */
      if (strncmp ((const char *) copy, pfx, pfxlen) != 0)
        {
          char *scan_ptr = copy;
          int drive_offset = 0;
          int matched = 0;

          /* Pattern 1: GCC format with line and column -> filename:line:col: */
          matched = sscanf (scan_ptr, "%" STRINGIFY(NFILEN) "[^:]:%d:%d: %n",
                            filename + drive_offset, &line, &column, &chars);

          /* Pattern 2: Standard format with line only -> filename:line: */
          if (matched < 2)
            {
              column = 0;
              matched = sscanf (scan_ptr, "%" STRINGIFY(NFILEN) "[^:]:%d: %n",
                                filename + drive_offset, &line, &chars);
            }

          /* Pattern 3: Cscope fallback space delimiter -> filename:line */
          if (matched < 2)
            {
              column = 0;
              matched = sscanf (scan_ptr, "%" STRINGIFY(NFILEN) "[^:\t ] %d %n",
                                filename + drive_offset, &line, &chars);
            }

          if (matched >= 2)
            {
              chars += drive_offset;

              /* Update cursor position in search/compiler buffer */
              curwp->w_dot.p = lp;
              curwp->w_dot.o = unslen (str, chars);
              curwp->w_flag |= WFMOVE;

              /* Ensure target file is accessible */
              if (access (filename, R_OK) != 0)
                {
                  eprintf ("Cannot read '%s'", filename);
                  free (copy);
                  return FALSE;
                }

              /* Open file in popup window or switch buffer */
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

              /* Jump to target line and column */
              if (gotoline (TRUE, line, 0) == FALSE)
                {
                  free (copy);
                  return FALSE;
                }

              wlen = wllength (curwp->w_dot.p);
              if (column > 0)
                curwp->w_dot.o = (column - 1 >= wlen) ? wlen : column - 1;
              else
                curwp->w_dot.o = 0;

              /* Display remainder of error/match message in echo line */
              len = len - chars;
              str = (uchar *) copy + chars;
              if (unslen (str, len) > ncol)
                {
                  len = uoffset (str, ncol);
                  str[len] = '\0';
                }
              eprintf ("%s", str);

              free (copy);
              return TRUE;
            }
        }
      lp = lforw (lp);
    }

  eprintf ("No error or match found");
  if (copy)
    free (copy);
  return FALSE;
}
