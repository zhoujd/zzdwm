/*
 * The functions in this file handle redisplay.
 * There are two halves, the ones that update the virtual display screen,
 * and the ones that make the physical display screen the same as the virtual
 * display screen.  These functions use hints that are left in the windows by
 * the commands.
 *
 */
#include <stdio.h>
#include "ed.h"
#include "color.h"
#ifdef __STDC__
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#else
#ifdef sparc
#define sparc_KR
#include <varargs.h>
#endif
#endif

#define	WFDEBUG	0			/* Window flag debug.		*/
#define	FASTHACK	1		/* Slightly faster update code	*/
/* JFM: move prompt definition from ed.h */
#define	PROMPT	" MicroEMACS 2.31 -- "

#ifdef COLOR
#ifdef UTF8
#define VCHAR int	/* attribute + char on 3 bytes */
#else
#define VCHAR short	/* attribute + char */
#endif
#else
#ifdef UTF8
#error "no UTF-8 support when no color"
#endif
#define VCHAR char
#endif

typedef	struct	VIDEO {
	short	v_flag;			/* Flags			*/
/*JFM*/	VCHAR	v_text[2];		/* Screen data.			*/
}	VIDEO;

#define	VFCHG	0x0001			/* Changed.			*/
#define	VFSTD	0x0002			/* Standout.			*/

int	sgarbf	= TRUE;			/* TRUE if screen is garbage	*/
int	mpresf	= FALSE;		/* TRUE if message in last line	*/
int	vtrow	= 0;			/* Row location of SW cursor	*/
static int vtcol = 0;			/* Column location of SW cursor	*/
int	ttrow	= HUGE;			/* Row location of HW cursor	*/
int	ttcol	= HUGE;			/* Column location of HW cursor	*/

static VIDEO	**vscreen;		/* Virtual screen.		*/
static VIDEO	**pscreen;		/* Physical screen.		*/
static int shift = 0;			/* JFM : shift for vtputc */

static void updateline(int row, VCHAR *vline, VCHAR *pline);
static void modeline(register WINDOW *wp);
static void mlputi(int i, int r);
static void mlputli(long l, int r);

/*
 * Initialize the data structures used by the display code.
 * The edge vectors used to access the screens are set up.
 * The operating system's terminal I/O channel is set up.
 * All the other things get initialized at compile time.
 * The original window has "WFCHG" set, so that it will get
 * completely redrawn on the first call to "update".
 */
void vtinit(void)
{
	register int	i, line_size;
	register VIDEO	*vp;

	topen();
	vscreen = (VIDEO **) malloc(term.t_nrow * sizeof(VIDEO *));
	if (!vscreen)
		abort();
	pscreen = (VIDEO **) malloc(term.t_nrow * sizeof(VIDEO *));
	if (!pscreen)
		abort();
/*JFM+*/
	line_size = sizeof(VIDEO) - sizeof vp->v_text
		+ term.t_ncol * sizeof vp->v_text[0];
	vp = (VIDEO *) malloc(line_size * term.t_nrow * 2);
	if (!vp)
		abort();
/*JFM-*/
	for (i = 0; i < term.t_nrow; ++i) {
		vscreen[i] = vp;
		vp = (VIDEO *) ((char *) vp + line_size);
		pscreen[i] = vp;
		vp = (VIDEO *) ((char *) vp + line_size);
	}
}

/*
 * Clean up the virtual terminal system, in anticipation for a return to the
 * operating system.  Move down to the last line and clear it out (the next
 * system prompt will be written in the line).  Shut down the channel to the
 * terminal.
 * JFM update:
 *	append a '\n' on the last line so the last message is not lost
 */
void vttidy(void)
{
	movecursor(term.t_nrow, 0);
/*JFM*/	tputc('\n');
/*JFM	teeol();	*/
	tstand(COLOR_NORMAL);
	tclose();
}

/*
 * Set the virtual cursor to the specified row and column on the
 * virtual screen.  There is no checking for nonsense values; this might
 * be a good idea during the early stages.
 * JFM : col is the display column shift
 */
static void vtmove(int row, int col)
{
	vtrow = row;
	vtcol = 0;			/* JFM : always physical column 0 */
	shift = col;			/* JFM : keep shift value for vtputc */
}

/*
 * Write a character to the virtual screen.  The virtual row and
 * column are updated.  If the line is too long put a "$" in the last column.
 * This routine only puts printing characters into the virtual terminal buffers.
 * Only column overflow is checked.
 * JFM : don't do anything if column is beyond shift value (set by vtmove).
 * 2.17: 'v' contains the attribute and the character
 */
#ifndef COLOR
static
#endif
void vtputc(register int v)
{
	register VIDEO	*vp;
	register int i;
#ifdef COLOR
	int c, a;

	c = v & CHAR_MASK;
	a = v & ATTR_MASK;
#else
#define c v
#define a 0
#endif

	vp = vscreen[vtrow];
	i = vtcol - shift;			/* physical column */
	if (i >= term.t_ncol) {
		if (i == term.t_ncol)
			vp->v_text[term.t_ncol - 1] = a + '$';
/*07*/		vtcol++;
	} else if (c == '\t') {
		do {
			vtputc(a + ' ');
		} while ((vtcol & 0x07) != 0);
	} else if (c < 0x20 || c == 0x7f) {
		vtputc(a + '^');
		vtputc(v ^ 0x40);
	} else	{
		if (i < 0) {
			if (vtcol == 0)
				vp->v_text[0] = a + '$';
		} else if (i != 0 || vtcol == 0) {
			vp->v_text[i] = v;
		}
		vtcol++;
	}
}
#ifndef COLOR
#undef a
#undef c
#endif

/*
 * Erase from the end of the software cursor to the end of the line on which
 * the software cursor is located.
 */
void vteeol(void)
{
	register VIDEO	*vp;
	register int i;

	vp = vscreen[vtrow];
/*JFM+*/
	i = vtcol - shift;
/*06*/	if (i <= 0) {
		i = 0;
/*06*/		if (vtcol > 0)
/*06*/			i++;			/* keep the '$' */
	}
	while (i < term.t_ncol)
#ifdef COLOR
		vp->v_text[i++] = (COLOR_NORMAL << ATTR_SHIFT) + ' ';
#else
		vp->v_text[i++] = ' ';
#endif
/*JFM-*/
}

/*+
 * Screen shift changes. (JFM)
 * Bound to C-X-< and C-X->.
 *-*/
int shiftleft(int f, int n)
{
	if (curwp->w_shift == 0)
		curwp->w_shift = 7;	/* one tabulation */
	if (curwp->w_shift < 100)
		curwp->w_shift += 8;
	curwp->w_flag |= WFHARD;
	return TRUE;
}
int shiftright(int f, int n)
{
	curwp->w_shift -= 8;
	if (curwp->w_shift < 0)
		curwp->w_shift = 0;
	curwp->w_flag |= WFHARD;
	return TRUE;
}

/*
 * Make sure that the display is right.  This is a three part process.
 * First, scan through all of the windows looking for dirty ones.
 * Check the framing, and refresh the screen.
 * Second, make sure that "currow" and "curcol" are correct for the current
 * window.
 * Third, make the virtual and physical screens the same.
 */
void update(void)
{
	register LINE	*lp;
		 LINE	*xlp;
	register WINDOW	*wp;
		VIDEO	*vp1;
	register int	i;
	register int	j;
	register int	c;
	register int physcol;			/* JFM : physical cursor col */

/*JFM+*/
	/* compute the position of the physical cursor */
	lp = curwp->w_dotp;
	physcol = 0;
	for (i = 0; i < curwp->w_doto; i++) {
		c = lgetc(lp, i);
		if (c == '\t')
			physcol |= 0x07;
		else if (c < 0x20 || c == 0x7f)
			++physcol;
#ifdef UTF8
		else if ((c & 0xc0) == 0x80)
			continue;
#endif
		++physcol;
	}
	curcol = physcol;			/* for goal column (basic.c) */
	physcol -= curwp->w_shift;		/* screen shift */
#if 1
/*2.17*/
	/* shift the window for the dot to be displayed */
	if (physcol < 0) {
		int shift;

		shift = (-physcol + 7) & ~7;
		physcol += shift;
		curwp->w_shift -= shift;
		curwp->w_flag |= WFHARD;
	} else if (physcol >= term.t_ncol) {
		shift = (physcol - term.t_ncol + 7) & ~7;
		physcol -= shift;
		curwp->w_shift += shift;
		curwp->w_flag |= WFHARD;
	}
#else
	if (physcol >= term.t_ncol)		/* Long line.		*/
		physcol = term.t_ncol-1;
	else if (physcol < 0)
		physcol = 0;
#endif
/*JFM-*/

	wp = wheadp;
	while (wp) {
		/* Look at any window with update flags set on.		*/
		if (wp->w_flag != 0) {
			/* If not force reframe, check the framing.	*/
			if (!(wp->w_flag & WFFORCE)) {
				lp = wp->w_linep;
				for (i = 0; i < wp->w_ntrows; ++i) {
					if (lp == wp->w_dotp)
						goto out;
					if (lp == wp->w_bufp->b_linep)
						break;
					lp = lforw(lp);
				}
			}
			/* Not acceptable, better compute a new value	*/
			/* for the line at the top of the window. Then	*/
			/* set the "WFHARD" flag to force full redraw.	*/
			i = wp->w_force;
			if (i > 0) {
				--i;
				if (i >= wp->w_ntrows)
					i = wp->w_ntrows - 1;
			} else if (i < 0) {
				i += wp->w_ntrows;
				if (i < 0)
					i = 0;
			} else
				i = wp->w_ntrows/2;
			lp = wp->w_dotp;
			while (i != 0 && lback(lp) != wp->w_bufp->b_linep) {
				--i;
				lp = lback(lp);
			}
			wp->w_linep = lp;
			wp->w_flag |= WFHARD;	/* Force full.		*/
		out:
			/* Try to use reduced update. Mode line update	*/
			/* has its own special flag. The fast update is	*/
			/* used if the only thing to do is within the	*/
			/* line editing.				*/
			lp = wp->w_linep;
			i  = wp->w_toprow;
			if ((wp->w_flag& ~ WFMODE) == WFEDIT) {
#ifdef COLOR
				int lastanchor;
#endif
				while (lp != wp->w_dotp) {
					++i;
					lp = lforw(lp);
				}
#ifdef	FASTHACK
				vscreen[i]->v_flag = VFCHG;
#else
				vscreen[i]->v_flag |= VFCHG;
				vscreen[i]->v_flag &= ~VFSTD;
#endif
/*JFM*/				vtmove(i, wp->w_shift);
#ifdef COLOR
				lastanchor = draw_line(wp->w_bufp, lp);
				vteeol();
				while (++i < wp->w_toprow + wp->w_ntrows
				    && (lp = lforw(lp)) != wp->w_bufp->b_linep
				    && lp->anchor != lastanchor) {
					lp->anchor = lastanchor;
					vtmove(i, wp->w_shift);
					lastanchor = draw_line(wp->w_bufp, lp);
					vteeol();
					vscreen[i]->v_flag = VFCHG;
				}
/*fixme: if last anchor changed, should parse all the next lines*/
#else
				for (j = 0; j < llength(lp); ++j)
					vtputc(lgetc(lp, j));
				vteeol();
#endif
			} else if ((wp->w_flag & (WFEDIT | WFHARD)) != 0) {
				while (i < wp->w_toprow+wp->w_ntrows) {
#ifdef	FASTHACK
					vscreen[i]->v_flag = VFCHG;
#else
					vscreen[i]->v_flag |= VFCHG;
					vscreen[i]->v_flag &= ~VFSTD;
#endif
/*JFM*/					vtmove(i, wp->w_shift);
					if (lp != wp->w_bufp->b_linep) {
#ifdef COLOR
						int lastanchor;

						lastanchor = draw_line(wp->w_bufp, lp);
#else
						for (j = 0; j < llength(lp); ++j)
							vtputc(lgetc(lp, j));
#endif
						lp = lforw(lp);
#ifdef COLOR
						if (lp != wp->w_bufp->b_linep)
							lp->anchor = lastanchor;
#endif
					}
					vteeol();
					++i;
				}
			}
#if	!WFDEBUG
			if ((wp->w_flag & WFMODE) != 0)
				modeline(wp);
			wp->w_flag  = 0;
			wp->w_force = 0;
#endif
		}		
#if	WFDEBUG
		modeline(wp);
		wp->w_flag =  0;
		wp->w_force = 0;
#endif
		/* Set standout mode on status line... */
		vscreen[wp->w_toprow+wp->w_ntrows]->v_flag |= VFSTD;
		wp = wp->w_wndp;
	}
	/* Always recompute the row and column number of the hardware	*/
	/* cursor.  This is the only update for simple moves.		*/
	xlp = lp = curwp->w_linep;
	currow = curwp->w_toprow;
	while (lp != curwp->w_dotp) {
		++currow;
		if (xlp == (lp = lforw(lp))) {	/* Fix infinite loop problem */
			currow = curwp->w_toprow;
			curwp->w_dotp = lp = xlp;
			curwp->w_doto = 0;
			break;
		}
	}
#if	GEM && NATIVE
	/* Special preparation for screen update on ATARI ST native screen */
	/* We shut off the cursor -- This speeds up the writing of text	*/
	/* quite a bit.							*/

	astcursor(0);			/* Turn cursor off for update	*/
#endif

	/* Special hacking if the screen is garbage. Clear the hardware	*/
	/* screen, and update your copy to agree with it. Set all the	*/
	/* virtual screen change bits, to force a full update.		*/

	if (sgarbf != FALSE) {
		for (i = 0; i < term.t_nrow; ++i) {
			vscreen[i]->v_flag |= VFCHG;
			vp1 = pscreen[i];
			for (j = 0; j < term.t_ncol; ++j)
#ifdef COLOR
				vp1->v_text[j] = (COLOR_NORMAL << ATTR_SHIFT)
							+ ' ';
#else
				vp1->v_text[j] = ' ';
#endif
		}
		movecursor(0, 0);		/* Erase the screen.	*/
		teeop();
		sgarbf = FALSE;			/* Erase-page clears	*/
		mpresf = FALSE;			/* the message area.	*/
	}

	/* Make sure that the physical and virtual displays agree.	*/
	/* Unlike before, the "updateline" code is only called with a	*/
	/* line that has been updated for sure.				*/

	for (i = 0; i < term.t_nrow; ++i) {
		vp1 = vscreen[i];
		if ((vp1->v_flag & VFCHG) != 0) {
			VIDEO	*vp2;

			vp2 = pscreen[i];
#ifndef COLOR
			if (vp1->v_flag & VFSTD) {	/* Standout mode */
#ifndef	FASTHACK
				vp1->v_flag &= ~VFSTD;
#endif
				tstand(COLOR_STATUS);
				updateline(i, &vp1->v_text[0], &vp2->v_text[0]);
				tstand(COLOR_NORMAL);
			} else
#endif
				updateline(i, &vp1->v_text[0], &vp2->v_text[0]);
#ifdef	FASTHACK
			vp1->v_flag = 0;
#else
			vp1->v_flag &= ~VFCHG;
#endif
		}
	}
	/* Finally, update the hardware cursor and flush out buffers.	*/
/*JFM*/	movecursor(currow, physcol);
#if	GEM && NATIVE
	astcursor(1);			/* Turn cursor back on...	*/
#endif
	tflush();
}

/*
 * Update a single line.  This does not know how to use insert or delete
 * character sequences; we are using VT52 functionality.  Update the physical
 * row and column variables.  It does try and exploit erase to end of line.
 */
static void updateline(int row,
			VCHAR *vline,
			VCHAR *pline)
{
	register VCHAR	*cp1;
	register VCHAR	*cp2;
	VCHAR	*cp3;
	VCHAR	*cp4;
	VCHAR	*cp5;
	register int	nbflag;
#ifdef COLOR
	unsigned int attr;
#endif

	cp1 = vline;				/* Compute left match.	*/
	cp2 = pline;
	while (*cp1 == *cp2) {
		++cp1;
		++cp2;
		/* This can still happen, even though we only call this routine	*/
		/* on changed lines. A hard update is always done when a line	*/
		/* splits, a massive change is done, or a buffer is displayed	*/
		/* twice. This optimizes out most of the excess updating. A lot	*/
		/* of computes are used, but these tend to be hard operations	*/
		/* that do a lot of update, so I don't really care.		*/
		if (cp1 == &vline[term.t_ncol])		/* All equal.		*/
			return;
	}

	nbflag = FALSE;
	cp3 = &vline[term.t_ncol];		/* Compute right match.	*/
	cp4 = &pline[term.t_ncol];
	while (cp3[-1] == cp4[-1]) {
		--cp3;
		--cp4;
		if ((cp3[0] & CHAR_MASK) != ' ') /* Note if any nonblank	*/
			nbflag = TRUE;		/* in right match.	*/
	}
	cp5 = cp3;
	if (!nbflag) {				/* Erase to EOL ?	*/
		while (cp5 != cp1 && (cp5[-1] & CHAR_MASK) == ' ')
			--cp5;
		if (cp3 - cp5 <= 3)		/* Use only if erase is	*/
			cp5 = cp3;		/* fewer characters.	*/
	}

	movecursor(row, cp1 - vline);		/* Go to start of line.	*/
#ifdef COLOR
	if (cp1 == vline)
		attr = COLOR_NORMAL << ATTR_SHIFT;
	else	{
		attr = cp1[-1] & ATTR_MASK;
		tstand(attr >> ATTR_SHIFT);
	}
#endif
	while (cp1 != cp5) {			/* Ordinary.		*/
#ifdef COLOR
		if (attr != (*cp1 & ATTR_MASK)) {
			attr = *cp1 & ATTR_MASK;
			tstand(attr >> ATTR_SHIFT);
		}
#ifdef UTF8
		if (*cp1 & 0x00ffff00) {	/* if multibytes */
			if (*cp1 & 0x00ff0000)	/* 3 bytes */
				tputc(*cp1 >> 16);
			tputc(*cp1 >> 8);
		}
#endif
#endif
		tputc(*cp1);
		++ttcol;
		*cp2++ = *cp1++;
	}
#ifdef COLOR
	if (attr != (COLOR_NORMAL << ATTR_SHIFT))
		tstand(COLOR_NORMAL);
#endif
	if (cp5 != cp3) {			/* Erase.		*/
		teeol();
		while (cp1 != cp3)
			*cp2++ = *cp1++;
	}
}

/*
 * Redisplay the mode line for the window pointed to by the "wp".
 * This is the only routine that has any idea of how the modeline is formatted.
 * You can change the modeline format by hacking at this routine.
 * Called by "update" any time there is a dirty window.
 */
static void modeline(register WINDOW *wp)
{
	register char	*cp;
	register int	c;
	register int	n;
	register BUFFER	*bp;
#ifdef COLOR
#define a (COLOR_STATUS << ATTR_SHIFT)
#else
#define a 0
#endif

	n = wp->w_toprow + wp->w_ntrows;	/* Location.		*/
	vscreen[n]->v_flag |= VFCHG;		/* Redraw next time.	*/
	vtmove(n, 0);				/* Seek to right line.	*/
	vtputc(a + '-');
	bp = wp->w_bufp;
	if (bp->b_flag & BFCHG)
		c = '*';			/* "*" if changed.	*/
	else if (bp->b_flag & (BFNOWRT | BFTEMP))
		c = '%';			/* "%" if readonly */
	else
		c = '-';
	vtputc(a + c);
	n  = 2;
	cp = PROMPT;				/* Buffer name.		*/
	while ((c = *cp++) != 0) {
		vtputc(a + c);
		++n;
	}
	cp = &bp->b_bname[0];
	while ((c = *cp++) != 0) {
		vtputc(a + c);
		++n;
	}
	vtputc(a + ' ');
	++n;
	if (bp->b_fname[0] != 0) {		/* File name.		*/
#if	LIBHELP
		if (bp->b_flag & BFHELP)
			cp = "- Subject: ";
		else
#endif
			cp = "-- File: ";
		while ((c = *cp++) != 0
		    && n < term.t_ncol) {
			vtputc(a + c);
			++n;
		}
		cp = &bp->b_fname[0];
		while ((c = *cp++) != 0
		    && n < term.t_ncol) {
			vtputc(a + c);
			++n;
		}
		if (n < term.t_ncol) {
			vtputc(a + ' ');
			++n;
		}
	}
#if	WFDEBUG
	vtputc('-');
	vtputc((wp->w_flag & WFMODE) != 0  ? 'M' : '-');
	vtputc((wp->w_flag & WFHARD) != 0  ? 'H' : '-');
	vtputc((wp->w_flag & WFEDIT) != 0  ? 'E' : '-');
	vtputc((wp->w_flag & WFMOVE) != 0  ? 'V' : '-');
	vtputc((wp->w_flag & WFFORCE) != 0 ? 'F' : '-');
	n += 6;
#endif
	while (n < term.t_ncol) {		/* Pad to full width.	*/
		vtputc(a + '-');
		++n;
	}
#undef a
}

/*
 * Send a command to the terminal to move the hardware cursor to row "row"
 * and column "col".  The row and column arguments are origin 0.
 * Optimize out random calls.  Update "ttrow" and "ttcol".
 */
void movecursor(int row, int col)
{
	if (row != ttrow || col != ttcol) {
		ttrow = row;
		ttcol = col;
		tmove(row, col);
	}
}

/*
 * Erase the message line.
 * This is a special routine because the message line is not considered to be
 * part of the virtual screen.  It always works immediately; the terminal
 * buffer is flushed via a call to the flusher.
 */
void mlerase(void)
{
	movecursor(term.t_nrow, 0);
	teeol();
	tflush();
	mpresf = FALSE;
}

/*
 * Ask a yes or no question in the message line.
 * Return either TRUE, FALSE, or ABORT.  The ABORT status is returned
 * if the user bumps out of the question with a ^G.
 * Used any time a confirmation is required.
 */
int mlyesno(char *prompt)
{
	char		buf[64];

	for (;;) {
		strcpy(buf, prompt);
		strcat(buf, " [y/n]? ");
#if 1
/*2.11*/
		mlwrite(buf);
		switch (tgetc()) {
		case 0x07:			/* ^G */
			return ABORT;
		case 'Y':
		case 'y':
			return TRUE;
		case 'N':
		case 'n':
			return FALSE;
		}
#else
		s = mlreply(buf, buf, sizeof(buf));
		if (s == ABORT)
			return ABORT;
		if (s != FALSE) {
			if (buf[0] == 'y' || buf[0] == 'Y')
				return TRUE;
			if (buf[0] == 'n' || buf[0] == 'N')
				return FALSE;
		}
#endif
	}
}

/*
 * Write a prompt into the message line, then read back a response.
 * Keep track of the physical position of the cursor.
 * If we are in a keyboard macro throw the prompt away, and return
 * the remembered response.  This lets macros run at full speed.
 * The reply is always terminated by a carriage return.
 * Handle erase, kill, and abort keys.
 */
int mlreply(char *prompt, char *buf, int nbuf)
{
	register int	cpos;
	register int	i;
	register int	c;

	cpos = 0;
	if (kbdmop) {
		while ((c = *kbdmop++) != '\0')
			buf[cpos++] = c;
		buf[cpos] = 0;
		if (buf[0] == 0)
			return FALSE;
		return TRUE;
	}
	mlwrite(prompt);
	for (;;) {
		c = tgetc();
		switch (c) {
		case 0x0d:			/* Return, end of line	*/
			buf[cpos++] = 0;
			if (kbdmip) {
				if (kbdmip + cpos > &kbdm[NKBDM - 3]) {
					ctrlg(FALSE, 0);
					tflush();
					return ABORT;
				}
				for (i = 0; i < cpos; ++i)
					*kbdmip++ = buf[i];
			}
			tputc('\r');
			ttcol = 0;
			tflush();
			if (buf[0] == 0)
				return FALSE;
			return TRUE;

		case 0x07:			/* Bell, abort		*/
			tputc('^');
			tputc('G');
			ttcol += 2;
			ctrlg(FALSE, 0);
			tflush();
			return ABORT;

		case 0x7f:			/* Rubout, erase	*/
		case 0x08:			/* Backspace, erase	*/
			if (cpos != 0) {
				tputc('\b');
				tputc(' ');
				tputc('\b');
				--ttcol;
				if (buf[--cpos] < 0x20) {
					tputc('\b');
					tputc(' ');
					tputc('\b');
					--ttcol;
				}
				tflush();
			}
			break;

		case 0x15:			/* C-U, kill		*/
			while (cpos != 0) {
				tputc('\b');
				tputc(' ');
				tputc('\b');
				--ttcol;
				if (buf[--cpos] < 0x20) {
					tputc('\b');
					tputc(' ');
					tputc('\b');
					--ttcol;
				}
			}
			tflush();
			break;

		default:
			if (cpos < nbuf-1) {
				buf[cpos++] = c;
				if (c < ' ') {
					tputc('^');
					++ttcol;
					c ^= 0x40;
				}
				tputc(c);
				++ttcol;
				tflush();
			}
		}
	}
}

/*
 * Write a message into the message line.
 * Keep track of the physical cursor position.
 * A small class of printf like format items is handled.
 * Assumes the stack grows down; this assumption is made by the "++"
 * in the argument scan loop.  Set the "message line" flag TRUE.
 */
#ifdef sparc_KR
void mlwrite(va_alist)
va_dcl
{
	register int	c;
	char	*fmt;
	va_list ap;

	va_start(ap);
	fmt = va_arg(ap, char *);
#else
#ifdef __STDC__
void mlwrite(char *fmt, ...)
{
	register int	c;
	va_list ap;

	va_start(ap, fmt);
#else
void mlwrite(char *fmt, int arg)
{
	register int	c;
	register char	*ap;

	ap = (char *) &arg;
#define va_arg(ap, type) ((type *)(ap += sizeof(type)))[-1]
#endif
#endif
	movecursor(term.t_nrow, 0);
	while ((c = *fmt++) != '\0'
	    && ttcol < term.t_ncol) {
		if (c != '%') {
			tputc(c);
			++ttcol;
		} else {
			c = *fmt++;
			switch (c) {
			case 'd':
				mlputi(va_arg(ap, int), 10);
				break;

			case 'o':
				mlputi(va_arg(ap, int),  8);
				break;

			case 'x':
				mlputi(va_arg(ap, int), 16);
				break;

			case 'D':
				mlputli(va_arg(ap, long), 10);
				break;

			case 's':
				mlputs(va_arg(ap, char *));
				break;

			default:
				tputc(c);
				++ttcol;
			}
		}
	}
	teeol();
	tflush();
	mpresf = TRUE;
}

/*
 * Write out a string.
 * Update the physical cursor position.  This assumes that the characters in the
 * string all have width "1"; if this is not the case things will get screwed up
 * a little.
 */
void mlputs(register char *s)
{
	register int	c;

	while ((c = *s++) != '\0'
	    && ttcol < term.t_ncol) {
		tputc(c);
		++ttcol;
	}
}

/*
 * Write out an integer, in the specified radix.
 * Update the physical cursor position.
 */
static char hexdigits[] = "0123456789abcdef";
static void mlputi(int i, int r)
{
	register int q;

	if (i < 0) {
		i = -i;
		tputc('-');
		++ttcol;
	}
	q = i / r;
	if (q != 0)
		mlputi(q, r);
	tputc(hexdigits[i % r]);
	++ttcol;
}

/*
 * do the same except as a long integer.
 */
static void mlputli(long l, int r)
{
	register long q;

	if (l < 0) {
		l = -l;
		tputc('-');
		++ttcol;
	}
	q = l / r;
	if (q != 0)
		mlputli(q, r);
	tputc(hexdigits[l % r]);
	++ttcol;
}
