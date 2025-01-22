/*
 * The routines in this file move the cursor around on the screen.
 * They compute a new value for the cursor, then adjust ".".
 * The display code always updates the cursor location, so only moves
 * between lines, or functions that adjust the top line in the window
 * and invalidate the framing, are hard.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include "ed.h"

static int getgoal(register LINE *dlp);

/*
 * Move the cursor to the beginning of the current line.
 * Trivial.
 */
int gotobol(int f, int n)
{
	curwp->w_doto = 0;
	return TRUE;
}

/*
 * Move the cursor backwards by "n" characters. If "n" is less than
 * zero call "forwchar" to actually do the move. Otherwise compute
 * the new cursor location. Error if you try and move out of the buffer.
 * Set the flag if the line pointer for dot changes.
 */
int backchar(int f, int n)
{
	register LINE *lp;

	if (n < 0)
		return forwchar(f, -n);
#ifdef UTF8
	lp = curwp->w_dotp;
#endif
	while (n--) {
		if (curwp->w_doto == 0) {
			if ((lp = lback(curwp->w_dotp)) == curbp->b_linep)
				return FALSE;
			curwp->w_dotp  = lp;
			curwp->w_doto  = llength(lp);
			curwp->w_flag |= WFMOVE;
			continue;
		}
		curwp->w_doto--;
#ifdef UTF8
		while ((lgetc(lp, curwp->w_doto) & 0xc0) == 0x80
		    && curwp->w_doto > 0)
			curwp->w_doto--;
#endif
	}
	return TRUE;
}

/*
 * Move the cursor to the end of the current line. Trivial.
 * No errors.
 */
int gotoeol(int f, int n)
{
	curwp->w_doto  = llength(curwp->w_dotp);
	return TRUE;
}

/*
 * Move the cursor forwards by "n" characters. If "n" is less than
 * zero call "backchar" to actually do the move. Otherwise compute
 * the new cursor location, and move ".".  Error if you try and move
 * off the end of the buffer. Set the flag if the line pointer
 * for dot changes.
 */
int forwchar(int f, int n)
{
#ifdef UTF8
	LINE *lp;
#endif

	if (n < 0)
		return backchar(f, -n);
#ifdef UTF8
	lp = curwp->w_dotp;
#endif
	while (n--) {
		if (curwp->w_doto == llength(curwp->w_dotp)) {
			if (curwp->w_dotp == curbp->b_linep)
				return FALSE;
			curwp->w_dotp = lforw(curwp->w_dotp);
#ifdef UTF8
			lp = curwp->w_dotp;
#endif
			curwp->w_doto = 0;
			curwp->w_flag |= WFMOVE;
		} else {
			curwp->w_doto++;
#ifdef UTF8
			while ((lgetc(lp, curwp->w_doto) & 0xc0) == 0x80
			    && curwp->w_doto < llength(curwp->w_dotp))
				curwp->w_doto++;
#endif
		}
	}
	return TRUE;
}

/*
 * Goto the beginning of the buffer.
 * Massive adjustment of dot. This is considered to be hard motion;
 * it really isn't if the original value of dot is the same as the
 * new value of dot.
 * Normally bound to "M-<".
 */
int gotobob(int f, int n)
{
	curwp->w_dotp  = lforw(curbp->b_linep);
	curwp->w_doto  = 0;
	curwp->w_flag |= WFHARD;
	return TRUE;
}

/*
 * Move to the end of the buffer.
 * Dot is always put at the end of the file (ZJ). The standard screen code does
 * most of the hard parts of update. Bound to "M->".
 */
int gotoeob(int f, int n)
{
	curwp->w_dotp  = curbp->b_linep;
	curwp->w_doto  = 0;
	curwp->w_flag |= WFHARD;
	return TRUE;
}

/*
 * Move forward by full lines.
 * If the number of lines to move is less than zero, call the backward line
 * function to actually do it. The last command controls how the goal column
 * is set. Bound to "C-N". No errors are possible.
 */
int forwline(int f, int n)
{
	register LINE	*dlp;

	if (n < 0)
		return backline(f, -n);
	if ((lastflag & CFCPCN) == 0)		/* Reset goal if last	*/
		curgoal = curcol;		/* not C-P or C-N	*/
	thisflag |= CFCPCN;
	dlp = curwp->w_dotp;
	while (n-- && dlp!=curbp->b_linep)
		dlp = lforw(dlp);
	curwp->w_dotp  = dlp;
	curwp->w_doto  = getgoal(dlp);
	curwp->w_flag |= WFMOVE;
	return TRUE;
}

/*
 * This function is like "forwline", but goes backwards.
 * The scheme is exactly the same.  Check for arguments that are
 * less than zero and call your alternate. Figure out the new line and
 * call "movedot" to perform the motion. No errors are possible.
 * Bound to "C-P".
 */
int backline(int f, int n)
{
	register LINE	*dlp;

	if (n < 0)
		return forwline(f, -n);
	if ((lastflag & CFCPCN) == 0)		/* Reset goal if the	*/
		curgoal = curcol;		/* last isn't C-P, C-N	*/
	thisflag |= CFCPCN;
	dlp = curwp->w_dotp;
	while (n-- && lback(dlp)!=curbp->b_linep)
		dlp = lback(dlp);
	curwp->w_dotp  = dlp;
	curwp->w_doto  = getgoal(dlp);
	curwp->w_flag |= WFMOVE;
	return TRUE;
}

/*
 * This routine, given a pointer to a LINE, and the current cursor goal
 * column, return the best choice for the offset. The offset is returned.
 * Used by "C-N" and "C-P".
 */
static int getgoal(register LINE *dlp)
{
	register int	c;
	register int	col;
	register int	dbo;

	col = 0;
	dbo = 0;
	while (dbo < llength(dlp)) {
		c = lgetc(dlp, dbo);
		if (c == '\t')
			col |= 0x07;
		else if (c < 0x20 || c == 0x7f)
			++col;			/* 2 characters */
		++col;
		if (col > curgoal)
			break;
		++dbo;
#ifdef UTF8
		while ((lgetc(dlp, dbo) & 0xc0) == 0x80
		    && dbo < llength(dlp))
			dbo++;
#endif
	}
	return dbo;
}

/*
 * Scroll forward by a specified number of lines, or by a full page if
 * no argument.
 * The "2" in the arithmetic on the window size is the overlap; this value is
 * the default overlap value in ITS EMACS. Because this zaps the top line in
 * the display window, we have to do a hard update.
 * Bound to "C-V".
 */
int forwpage(int f, int n)
{
	register LINE	*lp;

	if (f == FALSE) {
		n = curwp->w_ntrows - 2;	/* Default scroll.	*/
		if (n <= 0)			/* Forget the overlap	*/
			n = 1;			/* if tiny window.	*/
	} else if (n < 0)
		return backpage(f, -n);
#if	CVMVAS
	else					/* Convert from pages	*/
		n *= curwp->w_ntrows;		/* to lines.		*/
#endif
	lp = curwp->w_linep;			/* start from top of screen */
#if 0
/*JFM*/	n += curwp->w_ntrows / 2;		/* and center dot */
#endif
	while (n-- && lp!=curbp->b_linep)
		lp = lforw(lp);
#if 1 /*2.15*/
/*2.17: simplify*/
	if (lp != curbp->b_linep) {
		curwp->w_linep = lp;
		curwp->w_flag |= WFHARD;
	}
#else
/*JFM*/	if (lp == curbp->b_linep) {
		mlwrite("End of buffer");
		return TRUE;
	}
/*2.11*/
	curwp->w_linep = lp;
	curwp->w_flag |= WFHARD;
#endif
	curwp->w_dotp  = lp;
	curwp->w_doto  = 0;
	return TRUE;
}

/*
 * This command is like "forwpage", but it goes backwards.
 * The "2", like above, is the overlap between the two windows. The
 * value is from the ITS EMACS manual.  We do a hard update for exactly
 * the same reason.
 * Bound to "M-V".
 * JFM: set the cursor at the bottom of screen.
 */
int backpage(int f, int n)
{
	register LINE	*lp;

	if (f == FALSE) {
		n = curwp->w_ntrows - 2;	/* Default scroll.	*/
		if (n <= 0)			/* Don't blow up if the	*/
			n = 1;			/* window is tiny.	*/
	} else if (n < 0)
		return forwpage(f, -n);
#if	CVMVAS
	else					/* Convert from pages	*/
		n *= curwp->w_ntrows;		/* to lines.		*/
#endif
	lp = curwp->w_linep;			/* start from top of screen */
#if 0
/*JFM*/	n -= curwp->w_ntrows / 2;		/* and center dot */
#endif
	while (n-- && lback(lp) != curbp->b_linep)
		lp = lback(lp);
/*2.11*/
	curwp->w_dotp = curwp->w_linep;
	curwp->w_linep = lp;
/*	curwp->w_dotp  = lp; */
	curwp->w_doto  = 0;
	curwp->w_flag |= WFHARD;
	return TRUE;
}

/*
 * Set the mark in the current window to the value of "." in the window.
 * No errors are possible.
 * Bound to "M-.".
 */
int setmark(int f, int n)
{
	curwp->w_markp = curwp->w_dotp;
	curwp->w_marko = curwp->w_doto;
	mlwrite("[Mark set]");
	return TRUE;
}

/*
 * Swap the values of "." and "mark" in the current window.
 * This is pretty easy, because all of the hard work gets done by the
 * standard routine that moves the mark about. The only possible error is
 * "no mark".
 * Bound to "C-X C-X".
 */
int swapmark(int f, int n)
{
	register LINE	*odotp;
	register int	odoto;

	if (curwp->w_markp == NULL) {
		mlwrite("No mark in this window");
		return FALSE;
	}
	odotp = curwp->w_dotp;
	odoto = curwp->w_doto;
	curwp->w_dotp  = curwp->w_markp;
	curwp->w_doto  = curwp->w_marko;
	curwp->w_markp = odotp;
	curwp->w_marko = odoto;
	curwp->w_flag |= WFMOVE;
	return TRUE;
}

/*
 * Go to a specific line in the buffer, mostly for
 * looking up errors in C programs, which give the
 * error a line number. If an argument is present, then
 * it is the line number, else prompt for a line number
 * to use.
 */
int gotoline(int f, int n)
{
	register LINE	*clp;
	register int	s;
	char		buf[32];

	if (f == FALSE) {
		if ((s = mlreply("Goto line: ", buf, sizeof(buf))) != TRUE)
			return s;
		n = atoi(buf);
	}
	if (n <= 0) {
		mlwrite("Bad line");
		return FALSE;
	}
	clp = lforw(curbp->b_linep);		/* "clp" is first line	*/
	while (n != 1) {
		if (clp == curbp->b_linep) {
			mlwrite("Line too large");
			return FALSE;
		}
		clp = lforw(clp);
		--n;
	}
	curwp->w_dotp = clp;
	curwp->w_doto = 0;
	curwp->w_flag |= WFMOVE;
	return TRUE;
}

#if OLD_NUMBER
/*
 * Go to a specific line from the original file, mostly for
 * looking up errors in C programs, which give the
 * error a line number. If an argument is present, then
 * it is the line number, else prompt for a line number
 * to use.
 */
gotofline(int f, int n)
{
	register LINE	*clp;
	register int	s;
	char		buf[32];

	if (f == FALSE) {
		if ((s = mlreply("Goto old line: ", buf, sizeof(buf))) != TRUE)
			return s;
		n = atoi(buf);
	}
	if (n <= 0) {
		mlwrite("Bad line");
		return FALSE;
	}
	clp = lforw(curbp->b_linep);		/* "clp" is first line	*/
	while (n != l_number(clp)) {
		if (clp == curbp->b_linep) {
			mlwrite("Line not in buffer");
			return FALSE;
		}
		clp = lforw(clp);
	}
	curwp->w_dotp = clp;
	curwp->w_doto = 0;
	curwp->w_flag |= WFMOVE;
	return TRUE;
}
#endif

/*+
 * diff : difference between buffers
 * ----
 *
 * Compare text in two windows and stop on the first difference.
 * (JFM extension)
 * Bound to C-X C-D.
 *-*/
int diff(int f, int n)
{
	WINDOW *s_wp;
	BUFFER *s_bp;
	register LINE *lp1, *lp2;
/*08+*/	register int o1, o2;

	if ((s_wp = curwp->w_wndp) == NULL)
		s_wp = wheadp;
	if (s_wp == curwp) {
		mlwrite("Only window");
		return TRUE;
	}
	s_bp = s_wp->w_bufp;
	lp1 = curwp->w_dotp;
	o1 = curwp->w_doto;
	lp2 = s_wp->w_dotp;
	o2 = s_wp->w_doto;
	for (;;) {
/*2.11*/
		/* Ignore space differences */
		while (o1 < llength(lp1)
		       && (lp1->l_text[o1] == ' '
			   || lp1->l_text[o1] == '\t'
			   || lp1->l_text[o1] == '\r'))
			o1++;
		if (o1 == llength(lp1)) {
			lp1 = lforw(lp1);
			o1 = 0;
			if (lp1 == curbp->b_linep)
				break;
			continue;
		}
		while (o2 < llength(lp2)
		       && (lp2->l_text[o2] == ' '
			   || lp2->l_text[o2] == '\t'
			   || lp2->l_text[o2] == '\r'))
			o2++;
		if (o2 == llength(lp2)) {
			lp2 = lforw(lp2);
			o2 = 0;
			if (lp2 == s_bp->b_linep)
				break;
			continue;
		}

		if (o1 == llength(lp1)) {
			if (o2 != llength(lp2))
				break;
		}
		else if (o2 == llength(lp2))
			break;
		else	{
			if (lp1->l_text[o1] != lp2->l_text[o2])
				break;
			o1++;
			o2++;
		}
	}
	curwp->w_dotp = lp1;			/* return new positions */
	curwp->w_doto = o1;
	curwp->w_flag = WFMOVE;
	s_wp->w_dotp = lp2;
	s_wp->w_doto = o2;
	s_wp->w_flag = WFMOVE;
	return TRUE;
}
