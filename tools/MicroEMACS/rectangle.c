/*
 * Filename rectangle.c
 */

#include "def.h"
#include <stdlib.h>
#include <string.h>

/*
 * Rectangle editing: operations on the column range between an
 * anchored corner and point, on the display-cell grid (tabs and wide
 * characters expanded) that "wlcolumn" and "wlcoloffset" map.  The
 * commands share one dedicated clipboard, separate from the linear
 * kill buffer, which holds one NUL-terminated UTF-8 string per line
 * of the last rectangle copied or killed.
 *
 * A character belongs to the rectangle when its cells intersect the
 * column range, so a character straddling an edge is included whole.
 * The commands that edit the buffer first expand a tab that straddles
 * an edge into the spaces it renders as, so only the part of it
 * inside the rectangle goes; wide characters cannot be split and move
 * whole.  Lines shorter than the rectangle read as blanks when text
 * is copied, and only "open-rectangle" and "yank-rectangle" pad them
 * out with real spaces.
 *
 * Every edit goes through linsert and ldelete, which check the
 * read-only flag, save undo, and set the window flags.  Each edit
 * position is first anchored with a UMOVE undo record, as the region
 * commands do, so one undo replays the lines each at its own anchor
 * and reverses the whole rectangle command.
 */

static LINE *rect_lp;		/* Anchor line, NULL when none.	 */
static int rect_col;		/* Anchor display column.	 */

static uchar **rect_lines;	/* Clipboard slices.		 */
static int rect_count;

static int rectbounds (LINE **first, LINE **last, int *s_col, int *e_col);
static int rectsave (LINE *first, LINE *last, int s_col, int e_col);
static uchar *rectslice (LINE *lp, int s_col, int e_col);
static int rectcutline (LINE *lp, int s_col, int e_col, int blank);

/*
 * Free the clipboard slices.
 */
static void
rectfree (void)
{
  while (rect_count > 0)
    free (rect_lines[--rect_count]);
  free (rect_lines);
  rect_lines = NULL;
}

/*
 * The ordered corners of the rectangle: the anchor must still name a
 * line of this buffer, and the corners are normalized so that
 * "*first" is the upper line and "*s_col" the left column, both
 * columns display columns counted from zero.
 */
static int
rectbounds (LINE **first, LINE **last, int *s_col, int *e_col)
{
  LINE *lp;
  int col;

  if (rect_lp == NULL)
    {
      eprintf ("No rectangle anchor");
      return (FALSE);
    }
  /* The anchor line may have been deleted since the anchor was set;
     look for it, so a pointer to freed memory is never followed. */
  for (lp = lforw (curbp->b_linep); lp != curbp->b_linep; lp = lforw (lp))
    if (lp == rect_lp)
      break;
  if (lp == curbp->b_linep)
    {
      rect_lp = NULL;
      eprintf ("Rectangle anchor lost");
      return (FALSE);
    }
  if (curwp->w_dot.p == curbp->b_linep)
    {
      eprintf ("Point is past the last line");
      return (FALSE);
    }
  if (lcmp (rect_lp, curwp->w_dot.p) < 0)
    {
      *first = rect_lp;
      *last = curwp->w_dot.p;
    }
  else
    {
      *first = curwp->w_dot.p;
      *last = rect_lp;
    }
  col = wlcolumn (curwp->w_dot.p, curwp->w_dot.o);
  *s_col = rect_col < col ? rect_col : col;
  *e_col = rect_col > col ? rect_col : col;
  return (TRUE);
}

/*
 * One line's slice of the rectangle: the characters whose cells
 * intersect [s_col, e_col), padded with spaces to the rectangle's
 * width when the line ends first.  Returns a NUL-terminated
 * malloc'ed string, or NULL when memory ran out.  The line is not
 * modified; a character straddling an edge is taken whole.
 */
static uchar *
rectslice (LINE *lp, int s_col, int e_col)
{
  uchar *slice;
  int start, end;		/* Character offsets of the slice. */
  int bytes, pad;

  start = wlcoloffset (lp, s_col);
  end = wlcoloffset (lp, e_col);
  if (end < wllength (lp) && wlcolumn (lp, end) < e_col)
    ++end;			/* The last character straddles.  */
  bytes = wloffset (lp, end) - wloffset (lp, start);
  /* Blanks for the cells from where the characters end, or from the
     left edge on a line that never reaches it, to the right edge. */
  pad = e_col - wlcolumn (lp, end);
  if (wlcolumn (lp, end) < s_col)
    pad = e_col - s_col;
  if (pad < 0)
    pad = 0;
  slice = (uchar *) malloc (bytes + pad + 1);
  if (slice == NULL)
    return NULL;
  memcpy (slice, lgets (lp) + wloffset (lp, start), bytes);
  memset (slice + bytes, ' ', pad);
  slice[bytes + pad] = '\0';
  return slice;
}

/*
 * Read the whole rectangle into the clipboard, replacing what was
 * there.  The lines are read before any of them is edited, so a kill
 * saves the original characters.
 */
static int
rectsave (LINE *first, LINE *last, int s_col, int e_col)
{
  uchar **lines;
  int count;

  lines = NULL;
  count = 0;
  for (LINE *lp = first; ; lp = lforw (lp))
    {
      uchar *slice = rectslice (lp, s_col, e_col);
      uchar **grown = (uchar **) realloc (lines, (count + 1)
                                          * sizeof (uchar *));
      if (slice == NULL || grown == NULL)
        {
          /* realloc moved the array when it succeeded; free the
             slices through whichever copy holds them. */
          uchar **kept = grown != NULL ? grown : lines;
          free (slice);
          while (count > 0)
            free (kept[--count]);
          free (kept);
          eprintf ("Out of memory");
          return (FALSE);
        }
      lines = grown;
      lines[count++] = slice;
      if (lp == last)
        break;
    }
  rectfree ();
  rect_lines = lines;
  rect_count = count;
  return (TRUE);
}

/*
 * Expand the tab at character offset "off" of line "lp" into the
 * spaces it renders as, so a cut may split inside it.  Dot is left
 * on the last replacement space.  Growing the line may reallocate
 * it: the caller must take the line back from "curwp->w_dot.p"
 * afterwards, not keep using "lp".
 */
static int
rectexpandtab (LINE *lp, int off)
{
  int width = tabsize - wlcolumn (lp, off) % tabsize;

  curwp->w_dot.p = lp;
  curwp->w_dot.o = off;
  saveundo (UMOVE, &curwp->w_dot);
  if (ldelete (1, FALSE) == FALSE)
    return (FALSE);
  return linsert (width, ' ', NULLPTR);
}

/*
 * Cut the characters whose cells intersect [s_col, e_col) out of one
 * line, through the ordinary edit primitives.  When "blank" is TRUE
 * the cells are replaced by spaces instead of removed, so the line
 * keeps its width.  A line that never reaches the rectangle is left
 * alone.  Returns FALSE when the edit was refused.  The edits may
 * replace "lp" with a reallocated line; the caller must not use its
 * copy of "lp" afterwards.
 */
static int
rectcutline (LINE *lp, int s_col, int e_col, int blank)
{
  int start, end, bytes, cells;

  /* A tab straddling an edge goes as spaces, so only its cells
     inside the rectangle are cut. */
  for (int edge = 0; edge < 2; ++edge)
    {
      int col = edge == 0 ? s_col : e_col;
      int off = wlcoloffset (lp, col);
      if (off < wllength (lp) && wlgetc (lp, off) == '\t'
          && wlcolumn (lp, off) < col)
        {
          if (rectexpandtab (lp, off) == FALSE)
            return (FALSE);
          lp = curwp->w_dot.p;
        }
    }
  start = wlcoloffset (lp, s_col);
  end = wlcoloffset (lp, e_col);
  if (end < wllength (lp) && wlcolumn (lp, end) < e_col)
    ++end;			/* A wide character straddles.	 */
  if (start >= end)
    return (TRUE);		/* Line misses the rectangle.	 */
  bytes = wloffset (lp, end) - wloffset (lp, start);
  cells = wlcolumn (lp, end) - wlcolumn (lp, start);
  curwp->w_dot.p = lp;
  curwp->w_dot.o = start;
  saveundo (UMOVE, &curwp->w_dot);
  if (ldelete (bytes, FALSE) == FALSE)
    return (FALSE);
  if (blank)
    return linsert (cells, ' ', NULLPTR);
  return (TRUE);
}

/*
 * Toggle the rectangle anchor at point.  The operators take the
 * rectangle between the anchor and point, either way around, and the
 * anchor is dropped once it has been used.
 */
int
rectanchor (int f, int n, int k)
{
  (void) f;
  (void) n;
  (void) k;

  if (rect_lp != NULL)
    {
      rect_lp = NULL;
      eprintf ("[Rectangle anchor dropped]");
      return (TRUE);
    }
  if (curwp->w_dot.p == curbp->b_linep)
    {
      eprintf ("Cannot anchor past the last line");
      return (FALSE);
    }
  rect_lp = curwp->w_dot.p;
  rect_col = wlcolumn (curwp->w_dot.p, curwp->w_dot.o);
  eprintf ("[Rectangle anchored at column %d]", rect_col + 1);
  return (TRUE);
}

/*
 * Copy the rectangle between the anchor and point into the rectangle
 * clipboard, leaving the buffer alone.  Short lines contribute
 * blanks; they are not padded out in the buffer.
 */
int
rectcopy (int f, int n, int k)
{
  LINE *first, *last;
  int s_col, e_col;

  (void) f;
  (void) n;
  (void) k;

  if (rectbounds (&first, &last, &s_col, &e_col) == FALSE)
    return (FALSE);
  if (rectsave (first, last, s_col, e_col) == FALSE)
    return (FALSE);
  rect_lp = NULL;
  eprintf ("[Rectangle of %d lines copied]", rect_count);
  return (TRUE);
}

/*
 * Kill the rectangle: save it as "copy-rectangle" does, then remove
 * its characters from each line.  Point is left on the upper left
 * corner of what was the rectangle.
 */
int
rectkill (int f, int n, int k)
{
  LINE *lp, *first, *last, *above;
  int s_col, e_col;

  (void) f;
  (void) n;
  (void) k;

  if (rectbounds (&first, &last, &s_col, &e_col) == FALSE)
    return (FALSE);
  if (rectsave (first, last, s_col, e_col) == FALSE)
    return (FALSE);
  /* "above" names the upper left corner after the cuts: a cut that
     grows a line frees and replaces it, and this neighbor is never
     touched.  Likewise "next" is read before the line holding it
     can be cut. */
  above = lback (first);
  for (lp = first; ; )
    {
      LINE *next = lforw (lp);
      int done = lp == last;
      if (rectcutline (lp, s_col, e_col, FALSE) == FALSE)
        return (FALSE);
      if (done)
        break;
      lp = next;
    }
  rect_lp = NULL;
  curwp->w_dot.p = lforw (above);
  curwp->w_dot.o = wlcoloffset (curwp->w_dot.p, s_col);
  curwp->w_flag |= WFMOVE;
  eprintf ("[Rectangle of %d lines killed]", rect_count);
  return (TRUE);
}

/*
 * Blank the rectangle in place: its characters become spaces, one
 * per cell they occupied, so nothing on the line moves.  Point is
 * left at the column it was at.
 */
int
rectclear (int f, int n, int k)
{
  LINE *lp, *first, *last, *beyond;
  int before, col, s_col, e_col, count;

  (void) f;
  (void) n;
  (void) k;

  if (rectbounds (&first, &last, &s_col, &e_col) == FALSE)
    return (FALSE);
  /* Remember point by the neighbor outside the rectangle beside its
     line and by its column, because a blank that grows the line
     frees and replaces the line itself. */
  before = curwp->w_dot.p == first;
  beyond = before != FALSE ? lback (first) : lforw (last);
  col = wlcolumn (curwp->w_dot.p, curwp->w_dot.o);
  count = 0;
  for (lp = first; ; )
    {
      LINE *next = lforw (lp);
      int done = lp == last;
      if (rectcutline (lp, s_col, e_col, TRUE) == FALSE)
        return (FALSE);
      ++count;
      if (done)
        break;
      lp = next;
    }
  rect_lp = NULL;
  curwp->w_dot.p = before != FALSE ? lforw (beyond) : lback (beyond);
  curwp->w_dot.o = wlcoloffset (curwp->w_dot.p, col);
  curwp->w_flag |= WFMOVE;
  eprintf ("[Rectangle of %d lines cleared]", count);
  return (TRUE);
}

/*
 * Open the rectangle: insert one column of blanks per column of the
 * rectangle, shifting the text to the right of it.  Lines too short
 * to reach the rectangle are padded out to its left edge first.
 * Point is left on the upper left corner of the opened column.
 */
int
rectopen (int f, int n, int k)
{
  LINE *lp, *first, *last, *above;
  int s_col, e_col, count;

  (void) f;
  (void) n;
  (void) k;

  if (rectbounds (&first, &last, &s_col, &e_col) == FALSE)
    return (FALSE);
  above = lback (first);
  count = 0;
  for (lp = first; ; )
    {
      LINE *next = lforw (lp);
      int done = lp == last;
      int end_col = wlcolumn (lp, wllength (lp));
      curwp->w_dot.p = lp;
      if (end_col < s_col)
        {
          curwp->w_dot.o = wllength (lp);
          saveundo (UMOVE, &curwp->w_dot);
          if (linsert (s_col - end_col, ' ', NULLPTR) == FALSE)
            return (FALSE);
          lp = curwp->w_dot.p;	/* The pad may replace the line. */
        }
      curwp->w_dot.o = wlcoloffset (lp, s_col);
      saveundo (UMOVE, &curwp->w_dot);
      if (linsert (e_col - s_col, ' ', NULLPTR) == FALSE)
        return (FALSE);
      ++count;
      if (done)
        break;
      lp = next;
    }
  rect_lp = NULL;
  curwp->w_dot.p = lforw (above);
  curwp->w_dot.o = wlcoloffset (curwp->w_dot.p, s_col);
  curwp->w_flag |= WFMOVE;
  eprintf ("[Rectangle of %d lines opened]", count);
  return (TRUE);
}

/*
 * Yank the rectangle clipboard at point: each slice goes on the line
 * of point and the lines below, at point's column, padded out with
 * spaces when a line ends first.  The yank stops at the end of the
 * buffer.
 */
int
rectyank (int f, int n, int k)
{
  LINE *lp;
  int col, i;

  (void) f;
  (void) n;
  (void) k;

  if (rect_count == 0 || rect_lines == NULL)
    {
      eprintf ("No rectangle in the clipboard");
      return (FALSE);
    }
  if (curwp->w_dot.p == curbp->b_linep)
    {
      eprintf ("Cannot yank past the last line");
      return (FALSE);
    }
  lp = curwp->w_dot.p;
  col = wlcolumn (lp, curwp->w_dot.o);
  for (i = 0; i < rect_count; ++i)
    {
      int end_col, bytes;
      uchar *slice = rect_lines[i];
      LINE *next;

      if (lp == curbp->b_linep)
        {
          eprintf ("[Yanked %d of %d lines: end of buffer]", i,
                   rect_count);
          break;
        }
      next = lforw (lp);		/* Read before "lp" can be replaced. */
      end_col = wlcolumn (lp, wllength (lp));
      curwp->w_dot.p = lp;
      if (end_col < col)
        {
          curwp->w_dot.o = wllength (lp);
          saveundo (UMOVE, &curwp->w_dot);
          if (linsert (col - end_col, ' ', NULLPTR) == FALSE)
            return (FALSE);
          lp = curwp->w_dot.p;	/* The pad may replace the line. */
        }
      curwp->w_dot.o = wlcoloffset (lp, col);
      saveundo (UMOVE, &curwp->w_dot);
      bytes = strlen ((char *) slice);
      if (linsert (bytes, 0, (char *) slice) == FALSE)
        return (FALSE);
      lp = next;
    }
  curwp->w_flag |= WFMOVE;
  eprintf ("[Rectangle of %d lines yanked]", i);
  return (TRUE);
}

/*
 * Abort out of the prefix, as "abortotherwind" does for C-X 4.
 */
static int
abortrectangle (int f, int n, int k)
{
  int status = ctrlg (f, n, k);

  if (status == ABORT)
    eprintf ("[Aborted]");
  return status;
}

/*
 * The C-X r prefix, as in GNU Emacs: read the next key and run the
 * rectangle command it names.  The keys take the region verbs one
 * prefix deeper (C-W kill, M-W copy, C-y yank), with C-c clear and
 * C-o open from GNU Emacs's C-x r chords, and the anchor sits on
 * C-@, the set-mark key, as GNU's rectangles run between point and
 * mark.
 */
int
rectprefix (int f, int n, int k)
{
  typedef struct RECTKEY
  {
    int rk_key;			/* Key following C-X r.		 */
    int (*rk_funcp) (int f, int n, int k);
  }
  RECTKEY;

  static const RECTKEY rectkey[] =
  {
    {KCTRL | '@', rectanchor},
    {KCTRL | 'C', rectclear},
    {KCTRL | 'W', rectkill},
    {KCTRL | 'O', rectopen},
    {KMETA | 'W', rectcopy},
    {KCTRL | 'Y', rectyank},
    {KCTRL | 'G', abortrectangle}
  };
  int key;

  (void) k;
  if (kbdmop != NULL)
    key = *kbdmop++;
  else
    {
      key = getkey ();
      if (kbdmip != NULL)
        {
          if (kbdmip > &kbdm[NKBDM - 2])
            return ABORT;
          *kbdmip++ = key;
        }
    }

  for (unsigned int index = 0;
       index < sizeof (rectkey) / sizeof (rectkey[0]); index++)
    if (rectkey[index].rk_key == key)
      return rectkey[index].rk_funcp (f, n, key);
  eprintf ("Unknown C-X r command");
  return ABORT;
}
