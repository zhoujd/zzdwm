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

/* $Header: /home/bloovis/cvsroot/pe/display.c,v 1.2 2005-10-18 02:18:08 bloovis Exp $
 *
 * Name:	MicroEMACS
 *		Gosling style redisplay.
 * Version:	30
 * By:		rex::conroy
 *		decvax!decwrl!dec-rhea!dec-rex!conroy
 *
 * The functions in this file handle redisplay. The
 * redisplay system knows almost nothing about the editing
 * process; the editing functions do, however, set some
 * hints to eliminate a lot of the grinding. There is more
 * that can be done; the "vtputc" interface is a real
 * pig. Two conditional compilation flags; the GOSLING
 * flag enables dynamic programming redisplay, using the
 * algorithm published by Jim Gosling in SIGOA. The MEMMAP
 * changes things around for memory mapped video. With
 * both off, the terminal is a VT52.
 *
 * $Log: display.c,v $
 * Revision 1.2  2005-10-18 02:18:08  bloovis
 * Rename some things to avoid conflict with ncurses.
 *
 * Revision 1.1.1.1  2003/11/06 02:51:52  bloovis
 * Imported sources
 *
 * Revision 1.6  2003/05/14 23:09:20  malexander
 * (mouse_event): Rename to mouseevent to avoid conflict with
 * Win32 header files.
 *
 * Revision 1.5  2002/01/23 22:36:07  malexander
 * (mouse_button, mouse_row, mouse_column): New variables for
 * mouse support.
 * (update): Use ALLWIND macro for looping through window list.
 * (mouse_event): New function for handling mouse events; currently does
 * nothing but print row and column of event.
 *
 * Revision 1.4  2001/02/28 21:07:40  malexander
 * * def.h (POS): New structure for holding line position, which replaces
 * dot and mark variable pairs everywhere.
 *
 * Revision 1.3  2000/09/29 00:19:38  malexander
 * Numerous changes to eliminate warnings and add prototypes.
 *
 * Revision 1.2  2000/07/21 16:20:32  malexander
 * Reformatted with GNU indent.
 *
 * Revision 1.1.1.1  2000/07/14 19:23:10  malexander
 * Imported sources
 *
 * Revision 1.3  91/01/07  10:24:00  alexande
 * Remove C++ warnings.  Changes for variable tab size.
 *
 * Revision 1.3  90/10/23  17:13:54  alexande
 * Change vttext to unsigned, to allow IBM PC graphical characters.
 *
 * Revision 1.2  90/07/03  13:22:11  alexande
 * Changed video buffers to uchar, to avoid sign-extension problems when
 * using funny IBM PC characters that are >=80h.
 *
 *
 */
#include "def.h"

/*
 * You can change these back to the types
 * implied by the name if you get tight for space. If you
 * make both of them "int" you get better code on the VAX.
 * They do nothing if this is not Gosling redisplay, except
 * for change the size of a structure that isn't used.
 * A bit of a cheat.
 */
#define XCHAR  int
#define XSHORT int

/*
 * A video structure always holds
 * an array of characters whose length is equal to
 * the longest line possible. Only some of this is
 * used if "ncol" isn't the same as "NCOL".
 */
typedef struct
{
  short v_hash;			/* Hash code, for compares.     */
  short v_flag;			/* Flag word.                   */
  short v_color;		/* Color of the line.           */
  XSHORT v_cost;		/* Cost of display.             */
  wchar_t v_text[NCOL];		/* The actual characters.       */
#ifdef COLOR
  short v_attr[NCOL];		/* Color of each character.     */
#endif
}
VIDEO;

#define VFCHG   0x0001		/* Changed.                     */
#define VFHBAD  0x0002		/* Hash and cost are bad.       */

/*
 * SCORE structures hold the optimal
 * trace trajectory, and the cost of redisplay, when
 * the dynamic programming redisplay code is used.
 * If no fancy redisplay, this isn't used. The trace index
 * fields can be "char", and the score a "short", but
 * this makes the code worse on the VAX.
 */
typedef struct
{
  XCHAR s_itrace;		/* "i" index for track back.    */
  XCHAR s_jtrace;		/* "j" index for trace back.    */
  XSHORT s_cost;		/* Display cost.                */
}
SCORE;

int sgarbf = TRUE;		/* TRUE if screen is garbage.   */
int visflag = FALSE;		/* TRUE if show visable.        */
int vtrow = 0;			/* Virtual cursor row.          */
int vtcol = 0;			/* Virtual cursor column.       */
wchar_t *vttext;		/* &(vscreen[vtrow]->v_text[0]) */
#ifdef COLOR
short *vtattrs;			/* &(vscreen[vtrow]->v_attr[0]) */
static int vtattr = CTEXT;	/* Current virtual color.       */
#endif
int tthue = CNONE;		/* Current color.               */
int ttrow = HUGE;		/* Physical cursor row.         */
int ttcol = HUGE;		/* Physical cursor column.      */
int tttop = HUGE;		/* Top of scroll region.        */
int ttbot = HUGE;		/* Bottom of scroll region.     */

int leftcol = 0;		/* Left column of window        */
static int leftmargin;		/* Left margin of current line  */

VIDEO *vscreen[NROW - 1];	/* Edge vector, virtual.        */
VIDEO *pscreen[NROW - 1];	/* Edge vector, physical.       */
#if MEMMAP			/* don't need pscreen           */
VIDEO video[NROW - 1];		/* Actual screen data.          */
#else
VIDEO video[2 * (NROW - 1)];	/* Actual screen data.          */
#endif
VIDEO blanks;			/* Blank line image.            */
static uchar spaces[NCOL];	/* ASCII spaces.		*/

/*
 * These variables are set by the tty backends when a mouse event
 * arrives.  Rows and columns are 0-based screen positions in the
 * editor's (possibly page-stacked) coordinate space.
 */
int mouse_button;		/* 0=left, 1=middle, 2=right    */
int mouse_row;			/* screen row (0 = top)		*/
int mouse_column;		/* screen column (0 = left)	*/
int mouse_kind;			/* MOUSE_PRESS and friends	*/
int mouse_mods;			/* MOUSE_SHIFT/ALT/CTRL bits	*/
int mouse_clicks;		/* Clicks in one gesture	*/
int mouse_wheel_step = 3;	/* Lines per wheel scroll	*/
int mouseselect;			/* Region highlight is shown	*/
int osc52 = 1;			/* Copy regions to the clipboard */

static int mouse_dragging;	/* Left button drag in progress */
static int mouse_drag_moved;	/* Mouse moved during the drag  */
static EWINDOW *mouse_anchor_wp;	/* Window of the drag   */
static int mouse_anchor_row;	/* Screen row of the press      */
static int mouse_anchor_col;	/* Screen column of the press   */
static POS mouse_saved_mark;	/* Mark before the drag started */

#ifdef COLOR
static LINE *select_line1;	/* First line of the region.    */
static int select_off1;		/* Offset on the first line.    */
static LINE *select_line2;	/* Last line of the region.     */
static int select_off2;		/* Offset after the last char.  */
static int select_inside;	/* Drawn lines inside region.   */
#endif

#if GOSLING
/*
 * This matrix is written as an array because
 * we do funny things in the "setscores" routine, which
 * is very compute intensive, to make the subscripts go away.
 * It would be "SCORE	score[NROW][NROW]" in old speak.
 * Look at "setscores" to understand what is up.
 */
SCORE score[NROW * NROW];
#endif

/*
 * Forward declarations.
 */
static void vtputs (const uchar *s, int n);
static void vtputc (unsigned int c);
static void vttextmove (int row, int margin, int col);
static int vttextcol (int col);
static int vttextend (void);
static int decimal_width (int value);
static int is_buffer_end_marker (const LINE *lp, const BUFFER *bp);
static void update_line_number_cache (EWINDOW *wp);
static int window_gutter_width (const EWINDOW *wp);
static void draw_line_number (int row, int number, int width);
static void ucopy (VIDEO *vvp, VIDEO *pvp);
static void uline (int row, VIDEO *vvp, VIDEO *pvp);
static void modeline (EWINDOW *wp);
#ifdef COLOR
static void put_video_colors (wchar_t *text, short *attrs, int count);
static int selection_column (LINE * lp, int o);
static int selection_setup (EWINDOW * wp, LINE * first);
static void draw_selection (LINE * lp);
static void selection_step (LINE * lp);
#endif
#if GOSLING
static void hash (VIDEO *vp);
static void setscores (int offs, int size);
static void traceback (int offs, int size, int i, int j);
#endif

/*
 * Initialize the data structures used
 * by the display code. The edge vectors used
 * to access the screens are set up. The operating
 * system's terminal I/O channel is set up. Fill the
 * "blanks" array with ASCII blanks. The rest is done
 * at compile time. The original window is marked
 * as needing full update, and the physical screen
 * is marked as garbage, so all the right stuff happens
 * on the first call to redisplay.
 */
void
vtinit (void)
{
  register VIDEO *vp;
  register int i;

  if (nrow > NROW)
    nrow = NROW;
  if (ncol > NCOL)
    ncol = NCOL;

  ttopen ();
  ttinit ();
  vp = &video[0];
  for (i = 0; i < NROW - 1; ++i)
    {
      vscreen[i] = vp;
      ++vp;
#if MEMMAP
      /* don't need pscreen */
#else
      pscreen[i] = vp;
      ++vp;
#endif
    }
  blanks.v_color = CTEXT;
  wmemset (blanks.v_text, ' ', NCOL);
#ifdef COLOR
  for (i = 0; i < NCOL; ++i)
    blanks.v_attr[i] = CTEXT;
#endif
  memset (spaces, ' ', NCOL);
}

/*
 * Tidy up the virtual display system
 * in anticipation of a return back to the host
 * operating system. Right now all we do is position
 * the cursor to the last line, erase the line, and
 * close the terminal channel.
 */
void
vttidy (void)
{
  if (scriptmode)
    return;

  ttcolor (CTEXT);
  ttnowindow ();		/* No scroll window.    */
  ttmove (nrow - 1, 0);		/* Echo line.           */
  tteeol ();
  tttidy ();
  ttflush ();
  ttclose ();
}

/*
 * Move the virtual cursor to an origin
 * 0 spot on the virtual display screen.
 * Also store a character pointer to the start
 * of the current line, which makes "vtputc" a little bit
 * more efficient. No checking for errors.
 */
static void
vtmove (int row, int col)
{
  vtrow = row;
  vtcol = col;
  vttext = &(vscreen[vtrow]->v_text[0]);
#ifdef COLOR
  vtattrs = &(vscreen[vtrow]->v_attr[0]);
#endif
}

static void
vttextmove (int row, int margin, int col)
{
  leftmargin = margin;
  leftcol = col;
  vtmove (row, 0);
}

static int
vttextcol (int col)
{
  return leftmargin + col - leftcol;
}

static int
vttextend (void)
{
  return leftcol + ncol - leftmargin;
}

static void
vtput_display_char (unsigned int c)
{
#if defined(MINGW) || defined(_WIN32)
  int width = uwidth (c);
#else
  const int width = 1;
#endif

  if (width == 0)
    return;
  if (vtcol >= leftcol)
    {
      vttext[vttextcol (vtcol)] = c;
#ifdef COLOR
      vtattrs[vttextcol (vtcol)] = vtattr;
#endif
    }
  vtcol++;

  if (width == 2 && vtcol < vttextend ())
    {
      if (vtcol >= leftcol)
        {
          vttext[vttextcol (vtcol)] = 0;
#ifdef COLOR
          vtattrs[vttextcol (vtcol)] = vtattr;
#endif
        }
      vtcol++;
    }
}

/*
 * Write a character to the virtual display,
 * dealing with long lines and the display of unprintable
 * things like control characters. Also expand tabs every 8
 * columns. This code only puts printing characters into
 * the virtual display image. Special care must be taken when
 * expanding tabs. On a screen whose width is not a multiple
 * of 8, it is possible for the virtual cursor to hit the
 * right margin before the next tab stop is reached. This
 * makes the tab code loop if you are not careful.
 * Three guesses how we found this.
 */
void
vtputc_color (unsigned int c, int color)
{
#ifdef COLOR
  vtattr = color;
#else
  (void) color;
#endif
  if (vtcol >= vttextend ())
    {
      vttext[ncol - 1] = '$';
#ifdef COLOR
      vtattrs[ncol - 1] = vtattr;
#endif
    }
  else if (c == '\t')
    vtputs (spaces, tabsize - (vtcol % tabsize));
  else if (CISCTRL (c) != FALSE)
    {
      vtputc ('^');
      vtputc (c ^ 0x40);
    }
  else
    vtput_display_char (c);
}

static void
vtputc (unsigned int c)
{
#ifdef COLOR
  vtputc_color (c, vtattr);
#else
  vtputc_color (c, CTEXT);
#endif
}

/*
 * Write a string to the virtual display.  Essentially similar
 * to vtputc(), except that a string and count are the parameters
 * instead of a single character.
 */
static void
vtputs (const uchar *s, int n)
{
  wchar_t c;
  int ulen;
  const uchar *end = s + n;

  while (s < end)
    {
      c = ugetc (s, 0, &ulen);
      s += ulen;
      if (vtcol >= vttextend ())
        {
          vttext[ncol - 1] = '$';
#ifdef COLOR
          vtattrs[ncol - 1] = vtattr;
#endif
          return;
        }
      else if (c == '\t')
        vtputs (spaces, tabsize - (vtcol % tabsize));
      else if (c < 0x80 && CISCTRL (c) != FALSE)
        {
          vtputc ('^');
          vtputc (c ^ 0x40);
        }
      else
        vtput_display_char (c);
    }
}

/*
 * Put a null-terminated string out to the virtual screen.
 */
static void
vtstring (const char *s)
{
  vtputs ((const uchar *)s, strlen (s));
}

static int
decimal_width (int value)
{
  int width = 1;

  while (value >= 10)
    {
      value /= 10;
      ++width;
    }
  return width;
}

static int
is_buffer_end_marker (const LINE *lp, const BUFFER *bp)
{
  return lp != firstline (bp)
    && lp == lastline (bp)
    && llength (lp) == 0;
}

static void
update_line_number_cache (EWINDOW *wp)
{
  BUFFER *bp = wp->w_bufp;
  LINE *lp = firstline (bp);
  int number = 1;
  int lines;
  int hide_last_line;

  lines = 0;
  while (lp != bp->b_linep)
    {
      if (lp == wp->w_linep)
        wp->w_topline = number;
      ++number;
      ++lines;
      lp = lforw (lp);
    }
  hide_last_line = lines > 1 && is_buffer_end_marker (lastline (bp), bp);
  if (hide_last_line)
    --lines;
  if (wp->w_linep == bp->b_linep)
    wp->w_topline = 0;
  else if (hide_last_line && wp->w_linep == lastline (bp))
    wp->w_topline = lines + 1;
  wp->w_lwidth = decimal_width (lines) + 1;
}

static int
window_gutter_width (const EWINDOW *wp)
{
  int width;

  if (numberflag == FALSE)
    return 0;
  width = wp->w_lwidth;
  if (width >= ncol)
    width = ncol > 1 ? ncol - 1 : 0;
  return width;
}

static void
draw_line_number (int row, int number, int width)
{
  char text[32];
  int pad;
  int i;

  leftmargin = 0;
  leftcol = 0;
  vtmove (row, 0);
#ifdef COLOR
  vtattr = CTEXT;
#endif
  if (number <= 0)
    {
      for (i = 0; i < width; ++i)
        vtputc (' ');
      return;
    }
  pad = width - 1 - decimal_width (number);
  for (i = 0; i < pad; ++i)
    vtputc (' ');
  snprintf (text, sizeof (text), "%d", number);
  vtstring (text);
  vtputc (' ');
}

/*
 * Put a fill end of line with char.
 */
static void
vtepadc (char ch)
{
  register int count;
#ifdef COLOR
  int i;
#endif

  if (vtcol < leftcol)
    vtcol = leftcol;
  if ((count = ncol - leftmargin + leftcol - vtcol) <= 0)
    return;
  wmemset (&vttext[vttextcol (vtcol)], ch, count);
#ifdef COLOR
  for (i = 0; i < count; ++i)
    vtattrs[vttextcol (vtcol) + i] = vtattr;
#endif
  vtcol += count;
}

/*
 * Determine if top line, bottom line, or both are visible.
 */
static char*
vismsg (EWINDOW *wp)
{
  BUFFER *bp = wp->w_bufp;
  LINE *lp = wp->w_linep;
  int rows = wp->w_ntrows;
  char *msg = NULL;
  int numlines, predlines, ratio;
  char tline[32];

  while (rows--)
    {
      lp = lforw(lp);
      if (lp == bp->b_linep)
        {
          msg = "bot";
          break;
        }
    }
  if (lback (wp->w_linep) == wp->w_bufp->b_linep)
    {
      if (msg)
        if (wllength (wp->w_linep) == 0)
          msg = "emp";
        else
          msg = "all";
      else
        msg = "top";
    }
  if (!msg)
    {
      lp = lforw (bp->b_linep);
      numlines = 0;
      predlines = 0;
      while (lp != bp->b_linep)
        {
          if (lp == wp->w_linep) {
            predlines = numlines;
          }
          ++numlines;
          lp = lforw (lp);
        }
      if (wp->w_dot.p == bp->b_linep)
        {
          msg = "bot";
        }
      else
        {
          ratio = 0;
          if (numlines != 0)
            ratio = (100L * predlines) / numlines;
          if (ratio > 99)
            ratio = 99;
          snprintf (tline, sizeof (tline), "%2d%%", ratio);
          msg = tline;
        }
    }
  return strdup (msg);
}

/*
 * Erase from the end of the
 * software cursor to the end of the
 * line on which the software cursor is
 * located. The display routines will decide
 * if a hardware erase to end of line command
 * should be used to display this.
 */
static void
vteeol (void)
{
  vtepadc(' ');
}

#ifdef COLOR
/*
 * Set the color display flag. Clear the syntax cache when colors are
 * enabled, so lines edited while colors were disabled are reparsed.
 */
int
setcolor (int f, int n, int k)
{
  BUFFER *bp;
  EWINDOW *wp;

  colorflag = f ? (n != 0) : !colorflag;
  if (colorflag != FALSE)
    {
      ALLBUF (bp)
        syntax_cache_clear_buffer (bp);
    }
  ALLWIND (wp)
    wp->w_flag |= WFMODE | WFHARD;
  sgarbf = TRUE;
  eprintf ("[Color now %s]", colorflag ? "ON" : "OFF");
  update ();
  return (TRUE);
}
#endif

/*
 * Update the mode lines for all windows.
 */
void
updatemode (void)
{
  register EWINDOW *wp;

  ALLWIND (wp)			/* Update mode lines.   */
    wp->w_flag |= WFMODE;
}

/*
 * Make sure that the display is
 * right. This is a three part process. First,
 * scan through all of the windows looking for dirty
 * ones. Check the framing, and refresh the screen.
 * Second, make sure that "currow" and "curcol" are
 * correct for the current window. Third, make the
 * virtual and physical screens the same.
 */
void
update (void)
{
  register LINE *lp;
  register EWINDOW *wp;
  register VIDEO *vp1;
  register VIDEO *vp2;
  register int i;
  register int c;
  register int curcol;
  register int currow;
  int gutter;
  int text_width;
  int line_number;
  uchar *s, *end;
#if GOSLING
  register int hflag;
  register int offs;
  register int size;
#endif
#ifdef COLOR
  int syntax_state;
  int next_syntax_state;
#endif

  if (scriptmode)
    return;

  if (curmsgf != FALSE || newmsgf != FALSE)
    {
      ALLWIND (wp)		/* For all windows.     */
        wp->w_flag |= WFMODE;	/* Must do mode lines.  */
    }
  curmsgf = newmsgf;		/* Sync. up right now.  */

  if (numberflag != FALSE
      && ((curwp->w_flag & WFHARD) != 0 || curwp->w_lwidth == 0))
    update_line_number_cache (curwp);
  gutter = window_gutter_width (curwp);
  text_width = ncol - gutter;

  curcol = 0;			/* find current column  */
  lp = curwp->w_dot.p;		/* Cursor location.     */
  s = lgets (lp);
  end = (uchar *) wlgetcptr (lp, curwp->w_dot.o);
  while (s < end)
    {
      int ulen;

      c = ugetc (s, 0, &ulen);
      s += ulen;
      if (c == '\t')
        curcol += (tabsize - curcol % tabsize);
      else if (c < 0x80 && CISCTRL (c) != FALSE)
        curcol += 2;
      else
        curcol += uwidth(c);
    }
  if (curcol >= text_width + curwp->w_leftcol)
    {				/* need scroll right?   */
      curwp->w_leftcol = curcol - text_width / 2;
      curwp->w_flag |= WFHARD;	/* force redraw         */
    }
  else if (curcol < curwp->w_leftcol)
    {				/* need scroll left?    */
      if (curcol < text_width / 2)	/* near left end?       */
        curwp->w_leftcol = 0;	/* put left edge at 0   */
      else
        curwp->w_leftcol = curcol - text_width / 2;
      curwp->w_flag |= WFHARD;	/* force redraw         */
    }
  curcol -= curwp->w_leftcol;	/* adjust column        */
  curcol += gutter;

#if GOSLING
  hflag = FALSE;		/* Not hard.            */
#endif
  ALLWIND(wp)
    {
      if (wp->w_flag != 0)
        {			/* Need update.         */
#ifdef COLOR
          const SYNTAX *syntax = syntax_for_buffer (wp->w_bufp);
#endif
          if ((wp->w_flag & WFFORCE) == 0)
            {
              /* See if the dot is visible.		*/
              lp = wp->w_linep;
              for (i = 0; i < wp->w_ntrows; ++i)
                {
                  if (lp == wp->w_dot.p)
                    goto out;
                  if (lp == wp->w_bufp->b_linep)
                    break;
                  lp = lforw (lp);
                }
            }
          i = wp->w_force;	/* Reframe this one.    */
          if (i > 0)
            {
              --i;
              if (i >= wp->w_ntrows)
                i = wp->w_ntrows - 1;
            }
          else if (i < 0)
            {
              i += wp->w_ntrows;
              if (i < 0)
                i = 0;
            }
          else
            i = wp->w_ntrows / 2;
          lp = wp->w_dot.p;
          while (i != 0 && lp != firstline (wp->w_bufp))
            {
              --i;
              lp = lback (lp);
            }
          wp->w_linep = lp;
          wp->w_flag |= WFHARD;	/* Force full.          */
        out:
          lp = wp->w_linep;	/* Try reduced update.  */
          i = wp->w_toprow;
          if (numberflag != FALSE
              && ((wp->w_flag & WFHARD) != 0 || wp->w_lwidth == 0))
            update_line_number_cache (wp);
          gutter = window_gutter_width (wp);
          line_number = wp->w_topline;
#ifdef COLOR
          selection_setup (wp, lp);
#endif
#ifdef COLOR
          if ((wp->w_flag & WFEDIT) != 0 && syntax != NULL)
            {
              syntax_state = syntax_state_before (wp->w_bufp, wp->w_dot.p);
              next_syntax_state = syntax_state;
              syntax_line (syntax, lgets (wp->w_dot.p),
                           llength (wp->w_dot.p), &next_syntax_state, FALSE);
              if (next_syntax_state != syntax_state)
                wp->w_flag |= WFHARD;
            }
#endif
          if ((wp->w_flag & ~WFMODE) == WFEDIT)
            {
              while (lp != wp->w_dot.p)
                {
                  ++i;
                  ++line_number;
                  lp = lforw (lp);
                }
              vscreen[i]->v_color = CTEXT;
              vscreen[i]->v_flag |= (VFCHG | VFHBAD);
              if (gutter != 0)
                draw_line_number (i,
                                  lp != wp->w_bufp->b_linep
                                    && !is_buffer_end_marker (lp, wp->w_bufp)
                                    ? line_number : 0,
                                  gutter);
              vttextmove (i, gutter, wp->w_leftcol);
#ifdef COLOR
              if (syntax != NULL)
                {
                  syntax_state = syntax_state_before (wp->w_bufp, lp);
                  syntax_line (syntax, lgets (lp), llength (lp),
                               &syntax_state, TRUE);
                }
              else
#endif
                vtputs (lgets (lp), llength (lp));
              vteeol ();
#ifdef COLOR
              draw_selection (lp);
#endif
              leftcol = 0;
              leftmargin = 0;
            }
          else if ((wp->w_flag & (WFEDIT | WFHARD)) != 0)
            {
#if GOSLING
              hflag = TRUE;
#endif
#ifdef COLOR
              syntax_state = syntax != NULL
                               ? syntax_state_before (wp->w_bufp, lp)
                               : SYNTAX_STATE_NONE;
#endif
              while (i < wp->w_toprow + wp->w_ntrows)
                {
                  vscreen[i]->v_color = CTEXT;
                  vscreen[i]->v_flag |= (VFCHG | VFHBAD);
                  if (gutter != 0)
                    draw_line_number (i,
                                      lp != wp->w_bufp->b_linep
                                        && !is_buffer_end_marker (lp, wp->w_bufp)
                                        ? line_number : 0,
                                      gutter);
                  vttextmove (i, gutter, wp->w_leftcol);
                  if (lp != wp->w_bufp->b_linep)
                    {
#ifdef COLOR
                      if (syntax != NULL)
                        syntax_line (syntax, lgets (lp), llength (lp),
                                     &syntax_state, TRUE);
                      else
#endif
                        vtputs (lgets (lp), llength (lp));
#ifdef COLOR
                      draw_selection (lp);
                      selection_step (lp);
#endif
                      ++line_number;
                      lp = lforw (lp);
                    }
                  vteeol ();
                  ++i;
                }
              leftcol = 0;
              leftmargin = 0;
            }
          if ((wp->w_flag & (WFMODE | WFHARD)) != 0)
            modeline (wp);
          wp->w_flag = 0;
          wp->w_force = 0;
        }
    }
  lp = curwp->w_linep;		/* Cursor location.     */
  currow = curwp->w_toprow;
  while (lp != curwp->w_dot.p)
    {
      ++currow;
      lp = lforw (lp);
    }
  if (sgarbf != FALSE)
    {				/* Screen is garbage.   */
      sgarbf = FALSE;		/* Erase-page clears    */
      epresf = FALSE;		/* the message area.    */
      tttop = HUGE;		/* Forget where you set */
      ttbot = HUGE;		/* scroll region.       */
      tthue = CNONE;		/* Color unknown.       */
      ttcolor (CTEXT);		/* Force color change   */
      ttmove (0, 0);
      tteeop ();
      for (i = 0; i < nrow - 1; ++i)
        {
          uline (i, vscreen[i], &blanks);
          ucopy (vscreen[i], pscreen[i]);
        }
      ttmove (currow, curcol);
      ttflush ();
      return;
    }
#if GOSLING
  if (hflag != FALSE)
    {				/* Hard update?         */
      for (i = 0; i < nrow - 1; ++i)
        {			/* Compute hash data.   */
          hash (vscreen[i]);
          hash (pscreen[i]);
        }
      offs = 0;			/* Get top match.       */
      while (offs != nrow - 1)
        {
          vp1 = vscreen[offs];
          vp2 = pscreen[offs];
          if (vp1->v_color != vp2->v_color || vp1->v_hash != vp2->v_hash)
            break;
          uline (offs, vp1, vp2);
          ucopy (vp1, vp2);
          ++offs;
        }
      if (offs == nrow - 1)
        {			/* Might get it all.    */
          ttmove (currow, curcol);
          ttflush ();
          return;
        }
      size = nrow - 1;		/* Get bottom match.    */
      while (size != offs)
        {
          vp1 = vscreen[size - 1];
          vp2 = pscreen[size - 1];
          if (vp1->v_color != vp2->v_color || vp1->v_hash != vp2->v_hash)
            break;
          uline (size - 1, vp1, vp2);
          ucopy (vp1, vp2);
          --size;
        }
      if ((size -= offs) == 0)	/* Get screen size.     */
        abort ();
      setscores (offs, size);	/* Do hard update.      */
      traceback (offs, size, size, size);
      for (i = 0; i < size; ++i)
        ucopy (vscreen[offs + i], pscreen[offs + i]);
      ttmove (currow, curcol);
      ttflush ();
      return;
    }
#endif
  for (i = 0; i < nrow - 1; ++i)
    {				/* Easy update.         */
      vp1 = vscreen[i];
      vp2 = pscreen[i];
      if ((vp1->v_flag & VFCHG) != 0)
        {
          uline (i, vp1, vp2);
          ucopy (vp1, vp2);
        }
    }
  ttmove (currow, curcol);
  ttflush ();
}

#ifdef COLOR
static void
put_video_colors (wchar_t *text, short *attrs, int count)
{
  int i;
  int color = attrs[0];

#if defined(MINGW) || defined(_WIN32)
  if (ttputs_color (text, attrs, count) != FALSE)
    {
      ttcol += count;
      return;
    }
#endif
  ttcolor (color);
  for (i = 0; i < count; ++i)
    {
      if (attrs[i] != color)
        {
          color = attrs[i];
          ttcolor (color);
        }
      ttputc (text[i]);
      ttcol += uwidth (text[i]);
    }
}
#endif

#if !MEMMAP
static int
video_is_plain (const VIDEO *vp)
{
#ifdef COLOR
  int i;

  for (i = 0; i < ncol; ++i)
    if (vp->v_attr[i] != CTEXT)
      return FALSE;
  return TRUE;
#else
  (void) vp;
  return TRUE;
#endif
}
#endif

/*
 * Update a saved copy of a line,
 * kept in a VIDEO structure. The "vvp" is
 * the one in the "vscreen". The "pvp" is the one
 * in the "pscreen". This is called to make the
 * virtual and physical screens the same when
 * display has done an update.
 */
static void
ucopy (VIDEO *vvp, VIDEO *pvp)
{
  vvp->v_flag &= ~VFCHG;	/* Changes done.        */
#if MEMMAP
  /* don't use pscreen */
#else
  pvp->v_flag = vvp->v_flag;	/* Update model.        */
  pvp->v_hash = vvp->v_hash;
  pvp->v_cost = vvp->v_cost;
  pvp->v_color = vvp->v_color;
  memcpy (pvp->v_text, vvp->v_text, ncol * sizeof (pvp->v_text[0]));
#ifdef COLOR
  memcpy (pvp->v_attr, vvp->v_attr, ncol * sizeof (pvp->v_attr[0]));
#endif
#endif
}

#if defined(MINGW) || defined(_WIN32)
/*
 * Update a single line. This routine only
 * uses basic functionality (no insert and delete character,
 * but erase to end of line). The "vvp" points at the VIDEO
 * structure for the line on the virtual screen, and the "pvp"
 * is the same for the physical screen. Avoid erase to end of
 * line when updating CMODE color lines, because of the way that
 * reverse video works on most terminals.
 */
static void
uline (int row, VIDEO *vvp, VIDEO *pvp)
{
#if MEMMAP
#ifdef COLOR
  ttmove (row, 0);
  put_video_colors (vvp->v_text, vvp->v_attr, ncol);
#else
  ttcolor (vvp->v_color);
  putline (row + 1, 1, (const wchar_t *) &vvp->v_text[0]);
#endif
#else
  register wchar_t *cp1;
  register wchar_t *cp2;
  register wchar_t *cp3;
  register wchar_t *cp4;
  register wchar_t *cp5;
  register int nbflag;
  int visual_col1 = 0;
  int visual_len = 0;
  wchar_t *p;
#ifdef COLOR
  short *attr1;
  short *attr2;
  short *attr5;
#endif

  if (vvp->v_color != pvp->v_color)
    { 		/* Wrong color, do a full redraw. */
      ttmove (row, 0);
#ifdef COLOR
      put_video_colors (vvp->v_text, vvp->v_attr, ncol);
#else
      ttcolor (vvp->v_color);
      ttputs (vvp->v_text, ncol);
      ttcol += ncol;
#endif
      return;
    }
  cp1 = &vvp->v_text[0]; 	/* Compute left match. */
  cp2 = &pvp->v_text[0];
#ifdef COLOR
  attr1 = &vvp->v_attr[0];
  attr2 = &pvp->v_attr[0];
  while (cp1 != &vvp->v_text[ncol] && cp1[0] == cp2[0]
         && attr1[0] == attr2[0])
#else
  while (cp1 != &vvp->v_text[ncol] && cp1[0] == cp2[0])
#endif
    {
      ++cp1;
      ++cp2;
#ifdef COLOR
      ++attr1;
      ++attr2;
#endif
    }
  if (cp1 == &vvp->v_text[ncol]) 	/* All equal. */
    return;

  nbflag = FALSE;
  cp3 = &vvp->v_text[ncol]; /* Compute right match. */
  cp4 = &pvp->v_text[ncol];
#ifdef COLOR
  attr5 = &vvp->v_attr[ncol];
  while (cp3[-1] == cp4[-1] && attr5[-1] == pvp->v_attr[cp4 - pvp->v_text - 1])
#else
  while (cp3[-1] == cp4[-1])
#endif
    {
      --cp3;
      --cp4;
#ifdef COLOR
      --attr5;
#endif
      if (cp3[0] != ' ') 	/* Note non-blanks in the right match. */
        nbflag = TRUE;
    }
  cp5 = cp3; 		/* Is erase good? */
  if (nbflag == FALSE && vvp->v_color == CTEXT && video_is_plain (vvp))
    {
      while (cp5 != cp1 && cp5[-1] == ' ')
        --cp5;
      if ((int) (cp3 - cp5) <= tceeol)
        cp5 = cp3;
    }
  /* Calculate actual visual column offset */
  for (p = &vvp->v_text[0]; p < cp1; ++p)
    {
      if (*p != 0) /* Skip padding nulls if present */
        visual_col1 += uwidth(*p);
    }
  /* Calculate actual visual length of string to print */
  for (p = cp1; p < cp5; ++p)
    {
      if (*p != 0)
        visual_len += uwidth(*p);
    }
  /* Move cursor to correct visual column, not array index */
  ttmove (row, visual_col1);
#ifdef COLOR
  put_video_colors (cp1, attr1, (int) (cp5 - cp1));
#else
  ttcolor (vvp->v_color);
  /* Output string segment */
  ttputs (cp1, (int) (cp5 - cp1));
  ttcol += visual_len;
#endif
  if (cp5 != cp3) 	/* Do erase. */
    tteeol ();
#endif
}
#else
/*
 * Update a single line. This routine only
 * uses basic functionality (no insert and delete character,
 * but erase to end of line). The "vvp" points at the VIDEO
 * structure for the line on the virtual screen, and the "pvp"
 * is the same for the physical screen. Avoid erase to end of
 * line when updating CMODE color lines, because of the way that
 * reverse video works on most terminals.
 */
static void
uline (int row, VIDEO *vvp, VIDEO *pvp)
{
#if MEMMAP
#ifdef COLOR
  ttmove (row, 0);
  put_video_colors (vvp->v_text, vvp->v_attr, ncol);
#else
  ttcolor (vvp->v_color);
  putline (row + 1, 1, (const wchar_t *) &vvp->v_text[0]);
#endif
#else
  register wchar_t *cp1;
  register wchar_t *cp2;
  register wchar_t *cp3;
  register wchar_t *cp4;
  register wchar_t *cp5;
  register int nbflag;
#ifdef COLOR
  short *attr1;
  short *attr2;
#endif

  if (vvp->v_color != pvp->v_color)
    {				/* Wrong color, do a    */
      ttmove (row, 0);		/* full redraw.         */
#ifdef COLOR
      put_video_colors (vvp->v_text, vvp->v_attr, ncol);
#else
      ttcolor (vvp->v_color);
      ttputs (vvp->v_text, ncol);
      ttcol += ncol;
#endif
      return;
    }
  cp1 = &vvp->v_text[0];	/* Compute left match.  */
  cp2 = &pvp->v_text[0];
#ifdef COLOR
  attr1 = &vvp->v_attr[0];
  attr2 = &pvp->v_attr[0];
  while (cp1 != &vvp->v_text[ncol] && cp1[0] == cp2[0]
         && attr1[0] == attr2[0])
#else
  while (cp1 != &vvp->v_text[ncol] && cp1[0] == cp2[0])
#endif
    {
      ++cp1;
      ++cp2;
#ifdef COLOR
      ++attr1;
      ++attr2;
#endif
    }
  if (cp1 == &vvp->v_text[ncol])	/* All equal.           */
    return;
  nbflag = FALSE;
  cp3 = &vvp->v_text[ncol];	/* Compute right match. */
  cp4 = &pvp->v_text[ncol];
#ifdef COLOR
  while (cp3[-1] == cp4[-1]
         && vvp->v_attr[cp3 - vvp->v_text - 1]
            == pvp->v_attr[cp4 - pvp->v_text - 1])
#else
  while (cp3[-1] == cp4[-1])
#endif
    {
      --cp3;
      --cp4;
      if (cp3[0] != ' ')	/* Note non-blanks in   */
        nbflag = TRUE;		/* the right match.     */
    }
  cp5 = cp3;			/* Is erase good?       */
  if (nbflag == FALSE && vvp->v_color == CTEXT && video_is_plain (vvp))
    {
      while (cp5 != cp1 && cp5[-1] == ' ')
        --cp5;
      /* Alcyon hack */
      if ((int) (cp3 - cp5) <= tceeol)
        cp5 = cp3;
    }
  /* Alcyon hack */
  ttmove (row, (int) (cp1 - &vvp->v_text[0]));
#ifdef COLOR
  put_video_colors (cp1, attr1, (int) (cp5 - cp1));
#else
  ttcolor (vvp->v_color);
#if 0			/* old code */
  while (cp1 != cp5)
    {
      ttputc (*cp1++);
      ++ttcol;
    }
#else /* new code */
  ttputs (cp1, (int) (cp5 - cp1));
  ttcol += (cp5 - cp1);
#endif
#endif
  if (cp5 != cp3)		/* Do erase.            */
    tteeol ();
#endif
}
#endif

/*
 * Redisplay the mode line for
 * the window pointed to by the "wp".
 * This is the only routine that has any idea
 * of how the modeline is formatted. You can
 * change the modeline format by hacking at
 * this routine. Called by "update" any time
 * there is a dirty window.
 */
static void
modeline (EWINDOW *wp)
{
  register BUFFER *bp;
  register int n;
  const char *mname;
  int lchar;	/* character to draw line in buffer with */
  char lstr[32];

  n = wp->w_toprow + wp->w_ntrows;	/* Location.            */
  leftmargin = 0;
  leftcol = 0;
  vtmove (n, 0);		/* Seek to right line.  */
  vscreen[n]->v_flag |= (VFCHG | VFHBAD);	/* Recompute, display.  */
  vscreen[n]->v_color = CMODE;	/* Mode line color.     */
#ifdef COLOR
  vtattr = CMODE;
#endif
  if (wp == curwp)			/* mark the current buffer */
    lchar = '-';
  else
    lchar = ' ';
  snprintf (lstr, sizeof(lstr), " %c%c ", lchar, lchar);
  bp = wp->w_bufp;
  vtputc (lchar);
  if ((bp->b_flag & BFRO) != 0)	/* "%" if read-only    */
    vtputc ('%');
  else if ((bp->b_flag & BFCHG) != 0)	/* "*" if changed.      */
    vtputc ('*');
  else
    vtputc (lchar);
  vtputc (' ');
  vtstring ("MicroEMACS");
  mname = modename (bp);
#ifdef COLOR
  if (mname == NULL)
    mname = syntax_name_for_buffer (bp);
#endif
  if (mname != NULL)
    {
      vtstring (lstr);
      vtputc ('(');
      vtstring (mname);
      vtputc (')');
    }
  if (bp->b_bname[0] != 0)
    {				/* Buffer name.         */
      vtstring (lstr);
      vtstring (bp->b_bname);
    }
  if (bp->b_fname[0] != 0)
    {				/* File name.           */
      vtstring (lstr);
      vtstring ("File: ");
      vtstring (bp->b_fname);
    }
  if (curmsgf != FALSE		/* Message alert.       */
      && wp->w_wndp == NULL)
    {
      vtputc (' ');
      while (vtcol < ncol - 9)
        vtputc (lchar);
      vtputc (' ');
      vtstring ("[Msg]");
    }

  if (visflag != FALSE)		/* Show All/Top/Bot.    */
    {
      char *vis = vismsg (wp);
      if (vis != NULL)
        {
          vtputc (' ');
          while (vtcol < ncol - 7)
            vtputc (lchar);
          vtputc (' ');
          vtstring (vis);
          free (vis);
        }
    }

  vtputc (' ');
  vtepadc (lchar);		/* Pad out with char.  */
#ifdef COLOR
  vtattr = CTEXT;
#endif
}

#if GOSLING
/*
 * Compute the hash code for
 * the line pointed to by the "vp". Recompute
 * it if necessary. Also set the approximate redisplay
 * cost. The validity of the hash code is marked by
 * a flag bit. The cost understand the advantages
 * of erase to end of line. Tuned for the VAX
 * by Bob McNamara; better than it used to be on
 * just about any machine.
 */
static void
hash (VIDEO *vp)
{
  register int i;
  register int n;
  register wchar_t *s;

  if ((vp->v_flag & VFHBAD) != 0)
    {				/* Hash bad.            */
      s = &vp->v_text[ncol - 1];
      for (i = ncol; i != 0; --i, --s)
        if (*s != ' ')
          break;
      n = ncol - i;		/* Erase cheaper?       */
      if (n > tceeol)
        n = tceeol;
      vp->v_cost = i + n;	/* Bytes + blanks.      */
      for (n = 0; i != 0; --i, --s)
        {
          n = (n << 5) + n + *s;
#ifdef COLOR
          n = (n << 5) + n + vp->v_attr[i - 1];
#endif
        }
      vp->v_hash = n;		/* Hash code.           */
      vp->v_flag &= ~VFHBAD;	/* Flag as all done.    */
    }
}

/*
 * Compute the Insert-Delete
 * cost matrix. The dynamic programming algorithm
 * described by James Gosling is used. This code assumes
 * that the line above the echo line is the last line involved
 * in the scroll region. This is easy to arrange on the VT100
 * because of the scrolling region. The "offs" is the origin 0
 * offset of the first row in the virtual/physical screen that
 * is being updated; the "size" is the length of the chunk of
 * screen being updated. For a full screen update, use offs=0
 * and size=nrow-1.
 *
 * Older versions of this code implemented the score matrix by
 * a two dimensional array of SCORE nodes. This put all kinds of
 * multiply instructions in the code! This version is written to
 * use a linear array and pointers, and contains no multiplication
 * at all. The code has been carefully looked at on the VAX, with
 * only marginal checking on other machines for efficiency. In
 * fact, this has been tuned twice! Bob McNamara tuned it even
 * more for the VAX, which is a big issue for him because of
 * the 66 line X displays.
 *
 * On some machines, replacing the "for (i=1; i<=size; ++i)" with
 * i = 1; do { } while (++i <=size)" will make the code quite a
 * bit better; but it looks ugly.
 */
static void
setscores (int offs, int size)
{
  register SCORE *sp;
  register int tempcost;
  register int bestcost;
  register int j;
  register int i;
  register VIDEO **vp;
  register VIDEO **pp;
  register SCORE *sp1;
  register VIDEO **vbase;
  register VIDEO **pbase;

  vbase = &vscreen[offs - 1];	/* By hand CSE's.       */
  pbase = &pscreen[offs - 1];
  score[0].s_itrace = 0;	/* [0, 0]               */
  score[0].s_jtrace = 0;
  score[0].s_cost = 0;
  sp = &score[1];		/* Row 0, inserts.      */
  tempcost = 0;
  vp = &vbase[1];
  for (j = 1; j <= size; ++j)
    {
      sp->s_itrace = 0;
      sp->s_jtrace = j - 1;
      tempcost += (*vp)->v_cost;
      sp->s_cost = tempcost + tcinsl[j];
      ++vp;
      ++sp;
    }
  sp = &score[NROW];		/* Column 0, deletes.   */
  for (i = 1; i <= size; ++i)
    {
      sp->s_itrace = i - 1;
      sp->s_jtrace = 0;
      sp->s_cost = tcdell[i];
      sp += NROW;
    }
  sp1 = &score[NROW + 1];	/* [1, 1].              */
  pp = &pbase[1];
  for (i = 1; i <= size; ++i)
    {
      sp = sp1;
      vp = &vbase[1];
      for (j = 1; j <= size; ++j)
        {
          sp->s_itrace = i - 1;
          sp->s_jtrace = j;
          bestcost = (sp - NROW)->s_cost;
          if (j != size)	/* Cd(A[i])=0 @ Dis.    */
            bestcost += tcdell[1];
          tempcost = (sp - 1)->s_cost;
          tempcost += (*vp)->v_cost;
          if (i != size)	/* Ci(B[j])=0 @ Dsj.    */
            tempcost += tcinsl[1];
          if (tempcost < bestcost)
            {
              sp->s_itrace = i;
              sp->s_jtrace = j - 1;
              bestcost = tempcost;
            }
          tempcost = (sp - NROW - 1)->s_cost;
          if ((*pp)->v_color != (*vp)->v_color
              || (*pp)->v_hash != (*vp)->v_hash)
            tempcost += (*vp)->v_cost;
          if (tempcost < bestcost)
            {
              sp->s_itrace = i - 1;
              sp->s_jtrace = j - 1;
              bestcost = tempcost;
            }
          sp->s_cost = bestcost;
          ++sp;			/* Next column.         */
          ++vp;
        }
      ++pp;
      sp1 += NROW;		/* Next row.            */
    }
}

/*
 * Trace back through the dynamic programming cost
 * matrix, and update the screen using an optimal sequence
 * of redraws, insert lines, and delete lines. The "offs" is
 * the origin 0 offset of the chunk of the screen we are about to
 * update. The "i" and "j" are always started in the lower right
 * corner of the matrix, and imply the size of the screen.
 * A full screen traceback is called with offs=0 and i=j=nrow-1.
 * There is some do-it-yourself double subscripting here,
 * which is acceptable because this routine is much less compute
 * intensive then the code that builds the score matrix!
 */
static void
traceback (int offs, int size, int i, int j)
{
  register int itrace;
  register int jtrace;
  register int k;
  register int ninsl;
  register int ndraw;
  register int ndell;

  if (i == 0 && j == 0)		/* End of update.       */
    return;
  itrace = score[(NROW * i) + j].s_itrace;
  jtrace = score[(NROW * i) + j].s_jtrace;
  if (itrace == i)
    {				/* [i, j-1]             */
      ninsl = 0;		/* Collect inserts.     */
      if (i != size)
        ninsl = 1;
      ndraw = 1;
      while (itrace != 0 || jtrace != 0)
        {
          if (score[(NROW * itrace) + jtrace].s_itrace != itrace)
            break;
          jtrace = score[(NROW * itrace) + jtrace].s_jtrace;
          if (i != size)
            ++ninsl;
          ++ndraw;
        }
      traceback (offs, size, itrace, jtrace);
      if (ninsl != 0)
        {
          ttcolor (CTEXT);
          ttinsl (offs + j - ninsl, offs + size - 1, ninsl);
        }
      do
        {			/* B[j], A[j] blank.    */
          k = offs + j - ndraw;
          uline (k, vscreen[k], &blanks);
        }
      while (--ndraw);
      return;
    }
  if (jtrace == j)
    {				/* [i-1, j]             */
      ndell = 0;		/* Collect deletes.     */
      if (j != size)
        ndell = 1;
      while (itrace != 0 || jtrace != 0)
        {
          if (score[(NROW * itrace) + jtrace].s_jtrace != jtrace)
            break;
          itrace = score[(NROW * itrace) + jtrace].s_itrace;
          if (j != size)
            ++ndell;
        }
      if (ndell != 0)
        {
          ttcolor (CTEXT);
          ttdell (offs + i - ndell, offs + size - 1, ndell);
        }
      traceback (offs, size, itrace, jtrace);
      return;
    }
  traceback (offs, size, itrace, jtrace);
  k = offs + j - 1;
  uline (k, vscreen[k], pscreen[offs + i - 1]);
}
#endif	/* GOSLING */

/*
 * Convert a screen column relative to the start of a window's text
 * area into a Unicode character index into line "lp".  Wide
 * characters that would be split by the click are not entered; the
 * position stops before them.  Tabs count as one column.  A target
 * past the end of the line clamps to the end of the line.
 */
int
mouse_col_to_offset (LINE * lp, int target)
{
  int i;
  int col;
  int len;

  if (target < 0)
    target = 0;
  len = wllength (lp);
  col = 0;
  for (i = 0; i < len; ++i)
    {
      int w = uwidth (wlgetc (lp, i));

      if (w <= 0)
        w = 1;
      if (col + w > target)
        break;
      col += w;
    }
  return (i);
}

/*
 * Find the window that owns the given screen row, or NULL if the
 * row does not belong to a window (for example the echo line).
 * Windows are laid out top to bottom without gaps, so the first
 * window whose mode line is at or below the row owns it.
 */
static EWINDOW *
mouse_find_window (int row)
{
  EWINDOW *wp;

  for (wp = wheadp; wp != NULL; wp = wp->w_wndp)
    {
      if (row < wp->w_toprow + wp->w_ntrows)
        return wp;          /* Text rows of this window. */
      if (row == wp->w_toprow + wp->w_ntrows)
        return wp;          /* Mode line of this window.  */
    }
  return NULL;
}

/*
 * Turn off the selection highlight.  Every window whose mark is
 * set may be showing it, and needs a full repaint.
 */
void
endmouseselect (void)
{
  EWINDOW *wp;

  if (mouseselect == FALSE)
    return;
  mouseselect = FALSE;
  ALLWIND (wp)
    if (wp->w_mark.p != NULL)
      wp->w_flag |= WFHARD;
}

/*
 * Copy the region "rp" to the system clipboard with the OSC 52
 * escape sequence, so text copied or killed in me can be pasted
 * into other applications, even over ssh.  Called by the region
 * commands when the "osc52" variable is set; terminals that refuse
 * OSC 52 simply ignore the sequence.  Very large regions are
 * skipped: no terminal is happy with a megabyte-sized paste.
 * The region is measured in characters (r_size); the text is
 * collected in bytes, with newlines between lines, exactly the
 * way copyregion fills the kill buffer.
 */
void
osc52send (const REGION *rp)
{
  static const char b64tab[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char *buf;			/* Region text in UTF-8 bytes. */
  char *b64;			/* Base64 encoded text.        */
  LINE *linep;
  int loffs;
  int nbytes;			/* Total bytes collected.      */
  int rsize;			/* Characters left to collect. */
  int chunk;
  int i, o;

  if (osc52 == FALSE || rp->r_size > 1000 * 1000)
    return;

  /* First pass: count the bytes. */
  nbytes = 0;
  rsize = rp->r_size;
  linep = rp->r_pos.p;
  loffs = rp->r_pos.o;
  while (rsize > 0)
    {
      if (loffs == wllength (linep))
        {			/* End of line: a newline.     */
          nbytes++;
          linep = lforw (linep);
          loffs = 0;
          rsize--;
        }
      else
        {
          chunk = wllength (linep) - loffs;
          if (chunk > rsize)
            chunk = rsize;
          nbytes += unblen (wlgetcptr (linep, loffs), chunk);
          loffs += chunk;
          rsize -= chunk;
        }
    }

  /* Second pass: collect the bytes. */
  buf = malloc (nbytes != 0 ? nbytes : 1);
  if (buf == NULL)
    return;
  nbytes = 0;
  rsize = rp->r_size;
  linep = rp->r_pos.p;
  loffs = rp->r_pos.o;
  while (rsize > 0)
    {
      if (loffs == wllength (linep))
        {
          buf[nbytes++] = '\n';
          linep = lforw (linep);
          loffs = 0;
          rsize--;
        }
      else
        {
          const uchar *cptr;
          int bytes;

          chunk = wllength (linep) - loffs;
          if (chunk > rsize)
            chunk = rsize;
          cptr = wlgetcptr (linep, loffs);
          bytes = unblen (cptr, chunk);
          memcpy (&buf[nbytes], cptr, bytes);
          nbytes += bytes;
          loffs += chunk;
          rsize -= chunk;
        }
    }

  /* Base64 encode: every 3 bytes become 4 characters. */
  b64 = malloc (4 * ((long) nbytes + 2) / 3 + 1);
  if (b64 != NULL)
    {
      for (i = 0, o = 0; nbytes - i >= 3; i += 3, o += 4)
        {
          b64[o] = b64tab[(buf[i] >> 2) & 0x3f];
          b64[o + 1] = b64tab[((buf[i] & 0x03) << 4)
                              | ((buf[i + 1] >> 4) & 0x0f)];
          b64[o + 2] = b64tab[((buf[i + 1] & 0x0f) << 2)
                              | ((buf[i + 2] >> 6) & 0x03)];
          b64[o + 3] = b64tab[buf[i + 2] & 0x3f];
        }
      if (nbytes - i == 1)
        {
          b64[o] = b64tab[(buf[i] >> 2) & 0x3f];
          b64[o + 1] = b64tab[(buf[i] & 0x03) << 4];
          b64[o + 2] = '=';
          b64[o + 3] = '=';
          o += 4;
        }
      else if (nbytes - i == 2)
        {
          b64[o] = b64tab[(buf[i] >> 2) & 0x3f];
          b64[o + 1] = b64tab[((buf[i] & 0x03) << 4)
                              | ((buf[i + 1] >> 4) & 0x0f)];
          b64[o + 2] = b64tab[(buf[i + 1] & 0x0f) << 2];
          b64[o + 3] = '=';
          o += 4;
        }
      b64[o] = '\0';
      ttosc52 (b64);
    }
  free (buf);
}


#ifdef COLOR
/*
 * Show the selection while the region highlight is active: the
 * mark-to-point region of every window whose mark is set, painted
 * by update as the lines are drawn.  This block computes the
 * region once per window paint pass.
 *
 * Display column of character "o" on line "lp", with tabs
 * expanded the way the screen draws them.
 */
static int
selection_column (LINE * lp, int o)
{
  int col;
  int i;
  int len;

  col = 0;
  len = wllength (lp);
  for (i = 0; i < o && i < len; ++i)
    {
      int c = wlgetc (lp, i);
      int w = uwidth (c);

      if (w <= 0)
        w = 1;
      if (c == '\t')
        col += tabsize - (col % tabsize);
      else
        col += w;
    }
  return (col);
}

/*
 * Remember the region between the mark and the point of window
 * "wp", in buffer order, for selection highlighting; "first" is
 * the first line the window is about to draw.  Returns FALSE when
 * there is nothing to draw, and sets select_line1 to NULL so that
 * draw_selection knows it.
 */
static int
selection_setup (EWINDOW * wp, LINE * first)
{
  LINE *lp;
  POS *front;
  POS *back;

  select_line1 = NULL;
  select_inside = FALSE;
  if (mouseselect == FALSE || wp->w_mark.p == NULL)
    return (FALSE);

  if (wp->w_dot.p == wp->w_mark.p)
    {
      if (wp->w_dot.o == wp->w_mark.o)
        return (FALSE);             /* Empty region. */
      front = &wp->w_dot;
      back = &wp->w_mark;
      if (front->o > back->o)
        {
          POS *swap = front;

          front = back;
          back = swap;
        }
    }
  else
    {
      /* The mark comes after the point if walking forward from the
         point meets it before the end-of-buffer line. */
      LINE *endp = wp->w_bufp->b_linep;
      POS *dot = &wp->w_dot;
      POS *mark = &wp->w_mark;

      lp = dot->p;
      for (;;)
        {
          if (lp == mark->p)
            break;
          if (lp == endp)
            {
              POS *swap = dot;

              dot = mark;
              mark = swap;
              break;
            }
          lp = lforw (lp);
        }
      front = dot;
      back = mark;
    }
  select_line1 = front->p;
  select_off1 = front->o;
  select_line2 = back->p;
  select_off2 = back->o;

  /* The first drawn line may already be inside the region. */
  if (first == select_line1 || first == select_line2)
    return (TRUE);
  if (select_line1 != select_line2)
    for (lp = lforw (select_line1); lp != select_line2; lp = lforw (lp))
      if (lp == first)
        {
          select_inside = TRUE;
          break;
        }
  return (TRUE);
}

/*
 * Advance the selection walk after the line "lp" was drawn: the
 * line after the region's first line is inside the region, and
 * the line after its last line is outside again.  The last line
 * is tested first, because a region on a single line must leave
 * the walk outside.
 */
static void
selection_step (LINE * lp)
{
  if (lp == select_line2)
    select_inside = FALSE;
  else if (lp == select_line1)
    select_inside = TRUE;
}

/*
 * Paint the selection color over the cells of the row just drawn
 * on vtrow that fall inside the region set up by selection_setup.
 * "lp" is the line that was drawn.  Called with the drawing
 * leftmargin/leftcol still in effect.
 */
static void
draw_selection (LINE * lp)
{
  int c1;
  int c2;
  int cell;
  int last;

  if (select_line1 == NULL)
    return;
  if (lp == select_line1 && lp == select_line2)
    {
      c1 = selection_column (lp, select_off1);
      c2 = selection_column (lp, select_off2);
    }
  else if (lp == select_line1)
    {
      c1 = selection_column (lp, select_off1);
      c2 = selection_column (lp, wllength (lp));
    }
  else if (lp == select_line2)
    {
      c1 = 0;
      c2 = selection_column (lp, select_off2);
    }
  else if (select_inside != FALSE)
    {
      c1 = 0;
      c2 = selection_column (lp, wllength (lp));
    }
  else
    return;

  cell = leftmargin + c1 - leftcol;
  if (cell < leftmargin)
    cell = leftmargin;
  last = leftmargin + c2 - leftcol;
  if (last > ncol)
    last = ncol;
  for (; cell < last; ++cell)
    vscreen[vtrow]->v_attr[cell] = CSELECT;
}
#endif	/* COLOR */

/*
 * Make the window under the mouse the current window.
 */
static void
mouse_switch_window (EWINDOW * wp)
{
  if (wp != curwp)
    {
      curwp = wp;
      curbp = wp->w_bufp;
      updatemode ();
    }
}

/*
 * Move the point to the cell under the mouse in the text area of
 * the window below.  When "only" is not NULL the point moves only
 * within that window, so a drag stays in the window it started in.
 * Returns the window whose point moved, or NULL when the mouse is
 * not over text (below the last window, on a mode line, or outside
 * "only").
 */
static EWINDOW *
mouse_drop_dot (EWINDOW * only)
{
  EWINDOW *wp;
  LINE *lp;
  int rowoff;
  int col;

  wp = mouse_find_window (mouse_row);
  if (wp == NULL || (only != NULL && wp != only))
    return (NULL);
  if (mouse_row == wp->w_toprow + wp->w_ntrows)
    return (NULL);              /* Mode line, not text. */
  mouse_switch_window (wp);

  lp = wp->w_linep;
  rowoff = mouse_row - wp->w_toprow;
  while (rowoff-- > 0 && lforw (lp) != wp->w_bufp->b_linep)
    lp = lforw (lp);

  col = mouse_column - window_gutter_width (wp) + wp->w_leftcol;
  wp->w_dot.p = lp;
  wp->w_dot.o = mouse_col_to_offset (lp, col);
  wp->w_flag |= WFMOVE;
  return (wp);
}

/*
 * Select the word around the point of window "wp": the run of word
 * characters, or the run of blanks, that the point sits in.  The
 * selection stays on one line.  Returns FALSE when the point is at
 * the end of the line and there is nothing to select.
 */
static int
mouse_select_word (EWINDOW * wp)
{
  LINE *lp;
  int len;
  int o;
  int word;

  lp = wp->w_dot.p;
  len = wllength (lp);
  o = wp->w_dot.o;
  if (o >= len)
    return (FALSE);
  word = CISWORD (wlgetc (lp, o)) != FALSE;
  while (o > 0 && (CISWORD (wlgetc (lp, o - 1)) != FALSE) == word)
    --o;
  wp->w_mark.p = lp;
  wp->w_mark.o = o;
  o = wp->w_dot.o;
  while (o < len && (CISWORD (wlgetc (lp, o)) != FALSE) == word)
    ++o;
  wp->w_dot.o = o;
  return (TRUE);
}

/*
 * Select the line the point of window "wp" is on, from its first
 * to its last character.
 */
static void
mouse_select_line (EWINDOW * wp)
{
  wp->w_mark.p = wp->w_dot.p;
  wp->w_mark.o = 0;
  wp->w_dot.o = wllength (wp->w_dot.p);
}

/*
 * A middle click yanks the kill buffer at the clicked cell; a
 * right click extends the region to the clicked cell.  Returns
 * TRUE when the button was one of these, FALSE for the left
 * button.
 */
static int
mouse_button_click (void)
{
  EWINDOW *wp;

  if (mouse_button == 1)
    {
      endmouseselect ();
      if (mouse_drop_dot (NULL) != NULL)
        (void) yank (FALSE, 1, KRANDOM);
      return (TRUE);
    }
  if (mouse_button == 2)
    {
      wp = mouse_drop_dot (NULL);
      if (wp != NULL && wp->w_mark.p != NULL)
        {
          mouseselect = TRUE;
          wp->w_flag |= WFHARD;
        }
      else
        endmouseselect ();
      return (TRUE);
    }
  return (FALSE);
}

/*
 * Handle a mouse event.  The tty backend decodes the event into
 * mouse_kind, mouse_mods, mouse_button, mouse_row, and mouse_column.
 *
 * A wheel event scrolls the window under the mouse by
 * mouse-wheel-step lines.  A press in the text of a window moves the
 * point there and anchors a drag: the mark before the drag is
 * remembered and the mark is dropped on the pressed cell right
 * away, because terminals differ in whether they report motion
 * while the button is held.  Motion during the drag extends the
 * region, and the release position completes it even when no motion
 * was reported; a press and release on the same cell is a plain
 * click, and the saved mark goes back, so clicking does not disturb
 * an existing mark.  A click that the tty layer reports as one
 * complete event just moves the point.  A middle click yanks the
 * kill buffer at the clicked cell, a right click extends the
 * region to the clicked cell, and a double or triple click
 * selects the word or line at the clicked cell.  A press on a
 * mode line selects that window.  While a mouse selection is
 * active the mark-to-point region is drawn in the selection
 * color; any key but a mouse event ends that (see
 * endmouseselect).
 */
int
mouseevent (int f, int n, int k)
{
  EWINDOW *wp;

  if (mouse_kind == MOUSE_WHEEL)
    {
      wp = mouse_find_window (mouse_row);
      if (wp == NULL)
        wp = curwp;
      mouse_switch_window (wp);
      return mvupwind (FALSE,
                       mouse_button == 3 ? mouse_wheel_step
                                         : -mouse_wheel_step,
                       KRANDOM);
    }

  /* A release ends the drag.  A release away from the pressed cell
     completes the region at the release position even on terminals
     that report no motion while the button is held; a release on
     the pressed cell without motion was a plain click, so the
     saved mark goes back. */
  if (mouse_kind == MOUSE_RELEASE)
    {
      int keep;                  /* Region survives the release. */

      if (mouse_dragging && mouse_button == 0)
        {
          mouse_dragging = FALSE;
          keep = FALSE;
          if (mouse_drag_moved
              || mouse_row != mouse_anchor_row
              || mouse_column != mouse_anchor_col)
            {
              /* A release outside the window the drag started in
                 keeps whatever motion already selected. */
              keep = mouse_drop_dot (mouse_anchor_wp) != NULL
                     || mouse_drag_moved;
            }
          if (!keep)
            {
              /* Plain click after all: the saved mark goes back,
                 and the highlight with it. */
              endmouseselect ();
              if (mouse_saved_mark.p != NULL)
                mouse_anchor_wp->w_mark = mouse_saved_mark;
              else
                mouse_anchor_wp->w_mark.p = NULL;
            }
          else
            mouse_anchor_wp->w_flag |= WFHARD;
        }
      return (TRUE);
    }

  /* Idle motion without a drag is ignored. */
  if (mouse_kind == MOUSE_MOTION && !mouse_dragging)
    return (TRUE);

  /* A click reported as one event is a whole press/release
     gesture.  A middle click yanks the kill buffer at the clicked
     cell, a right click extends the region to the clicked cell, a
     double or triple click selects the word or line at the clicked
     cell, and a single left click just moves the point. */
  if (mouse_kind == MOUSE_CLICK)
    {
      mouse_dragging = FALSE;
      wp = mouse_find_window (mouse_row);
      if (wp == NULL)
        return (TRUE);
      if (mouse_row == wp->w_toprow + wp->w_ntrows)
        {
          mouse_switch_window (wp);
          endmouseselect ();
          return (TRUE);
        }
      if (mouse_button_click () != FALSE)
        return (TRUE);
      if (mouse_clicks >= 3)
        {
          wp = mouse_drop_dot (NULL);
          if (wp != NULL)
            {
              mouse_select_line (wp);
              mouseselect = TRUE;
              wp->w_flag |= WFHARD;
            }
          return (TRUE);
        }
      if (mouse_clicks == 2)
        {
          wp = mouse_drop_dot (NULL);
          if (wp != NULL && mouse_select_word (wp) != FALSE)
            {
              mouseselect = TRUE;
              wp->w_flag |= WFHARD;
            }
          else
            endmouseselect ();
          return (TRUE);
        }
      endmouseselect ();
      (void) mouse_drop_dot (NULL);
      return (TRUE);
    }

  /* A fresh press anchors a drag on the pressed cell; a middle or
     right press acts like the matching click, and a press on a
     mode line selects that window. */
  if (!mouse_dragging)
    {
      wp = mouse_find_window (mouse_row);
      if (wp == NULL)
        return (TRUE);
      if (mouse_row == wp->w_toprow + wp->w_ntrows)
        {
          mouse_switch_window (wp);
          endmouseselect ();
          return (TRUE);
        }
      if (mouse_button_click () != FALSE)
        return (TRUE);
      wp = mouse_drop_dot (NULL);
      if (wp == NULL)
        return (TRUE);
      mouse_dragging = TRUE;
      mouse_drag_moved = FALSE;
      mouse_anchor_wp = wp;
      mouse_anchor_row = mouse_row;
      mouse_anchor_col = mouse_column;
      mouse_saved_mark = wp->w_mark;
      wp->w_mark = wp->w_dot;
      mouseselect = TRUE;
      wp->w_flag |= WFHARD;
      return (TRUE);
    }

  /* Dragging: motion reports extend the region by moving the point
     within the window the drag started in. */
  if (mouse_drop_dot (mouse_anchor_wp) != NULL)
    {
      mouse_drag_moved = TRUE;
      mouse_anchor_wp->w_flag |= WFHARD;
    }
  return (TRUE);
}

/*
 * Show visable on modeline.
 */
int
showvisable (int f, int n, int k)
{
  visflag = !visflag;
  updatemode ();               /* Update mode lines    */
  return (TRUE);
}

/*
 * Set the line number display flag.
 */
int
setnumber (int f, int n, int k)
{
  EWINDOW *wp;

  numberflag = f ? (n != 0) : !numberflag;
  ALLWIND (wp)
    {
      wp->w_lwidth = 0;
      wp->w_topline = 0;
      wp->w_flag |= WFHARD;
    }
  sgarbf = TRUE;
  eprintf ("[Line numbers now %s]", numberflag ? "ON" : "OFF");
  update ();
  return (TRUE);
}
