/*
 * This file contains the command processing functions for a number of 
 * random commands.  There is no functional grouping here, for sure.
 */
#include	<stdio.h>
#include	"ed.h"

static int tabsize;			/* Tab size (0: use real tabs)	*/

/*
 * Return current column.
 */
int getccol(void)
{
	register int c, i;
	register int col = 0;
	register WINDOW *lcurwp;

	lcurwp = curwp;
	for (i = 0; i < lcurwp->w_doto; ++i) {
		c = lgetc(lcurwp->w_dotp, i);
		if (c == '\t')
			col |= 0x07;
		else if (c < 0x20 || c == 0x7f)
			++col;
#ifdef UTF8
		else if ((c & 0xc0) == 0x80)
			continue;
#endif
		++col;
	}
	return col;
}

/*
 * Set fill column to n, if given, otherwise use the current cursor
 * column. Either way, tell user where the fill column really is. 
 */
int setfillcol(int f, int n)
{
	if (n < 2)
		fillcol = getccol();
	else
		fillcol = n - 1;
	mlwrite("[Wrap at column %d]", fillcol + 1);
	return TRUE;
}

/*
 * Display the current position of the cursor,
 * in origin 1 X-Y coordinates, the character that is
 * under the cursor (in hex), and the fraction of the
 * text that is before the cursor. The displayed column
 * is not the current column, but the column that would
 * be used on an infinite width display. Normally this
 * is bound to "C-X =".
 */
int showcpos(int f, int n)
{
	register LINE	*clp;
	register long	nch = 0L;
	register long	nbc = 0L;
	register int	cbo = 0;
	register int	linecnt = 1;
	register int	cac = 0;
	register int	curline = 0;
	int		ratio;
	int		col;

	clp = lforw(curbp->b_linep);		/* Grovel the data.	*/
#if 1
	while (1) {
		if (clp == curwp->w_dotp) {
			cbo = curwp->w_doto;
			curline = linecnt;
			nbc = nch + cbo;
			if (cbo == llength(clp)) {
				cac = '\n';
			} else {
				cac = lgetc(clp, cbo);
#ifdef UTF8
				if (cac & 0x80) {
					int c;

					while (cbo < llength(clp) - 1) {
						c = lgetc(clp, ++cbo);
						if ((c &0xc0) != 0x80)
							break;
						cac <<= 8;
						cac |= c;
					}
				}
#endif
			}
		}
		if (clp == curbp->b_linep)
			break;
		nch += llength(clp) + 1;
		++linecnt;
		clp = lforw(clp);
	}
#else
	for (;;) {
		if (clp == curwp->w_dotp && cbo == curwp->w_doto) {
			curline = linecnt;
			nbc = nch;
			if (cbo == llength(clp))
				cac = '\n';
			else
				cac = lgetc(clp, cbo);
		}
		if (cbo == llength(clp)) {
			if (clp == curbp->b_linep)
				break;
			clp = lforw(clp);
			++linecnt;
			cbo = 0;
		} else
			++cbo;
		++nch;
	}
#endif
	col = getccol() + 1;			/* Get real column.	*/
	ratio = 0;				/* Ratio before dot.	*/
	if (nch != 0)
		ratio = (100L * nbc) / nch;
	mlwrite("X=%d Y=%d CH=0x%x .=%D (%d%% of %D) line %d of %d",
		col, currow + 1, cac, nbc, ratio, nch, curline, linecnt);
	return TRUE;
}

/*
 * Twiddle the two characters on either side of
 * dot. If dot is at the end of the line twiddle the
 * two characters before it. Return with an error if dot
 * is at the beginning of line; it seems to be a bit
 * pointless to make this work. This fixes up a very
 * common typo with a single stroke. Normally bound
 * to "C-T". This always works within a line, so
 * "WFEDIT" is good enough.
 */
int twiddle(int f, int n)
{
	register WINDOW *lcurwp;
	register LINE	*dotp;
	register int	doto;
	register unsigned char cl, cr;
#ifdef UTF8
	long tmp;
	int ncr, ncl, i;
#endif

	if (!lchange(WFEDIT))
		return FALSE;
	lcurwp = curwp;
	dotp = lcurwp->w_dotp;
	doto = lcurwp->w_doto;
	if (doto == llength(dotp)) {
		if (--doto < 0)
			return FALSE;
	}
	cr = lgetc(dotp, doto);
#ifdef UTF8
	ncr = ncl = 0;
	if (cr >= 0x80) {
		if (cr >= 0xc0) {	/* if utf-8 sequence start */
			if (doto + 1 >= llength(dotp))
				return FALSE;
			ncr++;
			if (doto + 2 < llength(dotp)) {
				cr = lgetc(dotp, doto + 2);
				if ((cr & 0xc0) == 0x80)
					ncr++;	/* sequences of 3 bytes max */
			}
		} else {		/* if twiddle at EOLN */
			do {
				if (--doto < 0)
					return FALSE;
				ncr++;
				cr = lgetc(dotp, doto);
			} while ((cr & 0xc0) == 0x80);
		}
	}
	if (--doto < 0)
		return FALSE;
	cl = lgetc(dotp, doto);
	while ((cl & 0xc0) == 0x80) {	/* search the start of utf-8 seq */
		ncl++;
		if (--doto < 0)
			return FALSE;
		cl = lgetc(dotp, doto);
	}
	if (ncr > 0) {
		lcurwp->w_doto = doto + ncr + 1;
		if (ncl == 0) {		/* one char on the left */
			while (ncr >= 0) {
				cr = lgetc(dotp, doto + 1);
				lputc(dotp, doto, cr);
				doto++;
				ncr--;
			}
			lputc(dotp, doto, cl);
			return TRUE;
		}

		/* many chars on both sides */
		tmp = 0;
		i = ncl;
		while (i >= 0) {
			tmp <<= 8;
			cl = lgetc(dotp, doto + i);
			tmp |= cl;
			i--;
		}
		while (ncr >= 0) {
			cr = lgetc(dotp, doto + ncl + 1);
			lputc(dotp, doto, cr);
			doto++;
			ncr--;
		}
		while (ncl >= 0) {
			lputc(dotp, doto, tmp);
			doto++;
			tmp >>= 8;
			ncl--;
		}
		return TRUE;
	}
	if (ncl > 0) {			/* one char on the right */
		lcurwp->w_doto = doto + 1;
		while (ncl >= 0) {
			cl = lgetc(dotp, doto + ncl);
			lputc(dotp, doto + ncl + 1, cl);
			ncl--;
		}
		lputc(dotp, doto, cr);
		return TRUE;
	}
#else
	if (--doto < 0)
		return FALSE;
	cl = lgetc(dotp, doto);
#endif
	lputc(dotp, doto + 0, cr);
	lputc(dotp, doto + 1, cl);
	return TRUE;
}

/*
 * Quote the next character, and insert it into the buffer.  All the characters
 * are taken literally, with the exception of the newline, which always has
 * its line splitting meaning.  The character is always read, even if it is
 * inserted 0 times, for regularity.  If executing a macro, get the character
 * from memory, otherwise, get it from the keyboard.
 * Bound to "M-Q" (for me) and "C-Q" (for Rich, and only on terminals
 * that don't need XON-XOFF).
 */
int quote(int f, int n)
{
	register int	s;
	register int	c;

	if (kbdmop != NULL)		/* if in a macro, get from memory */
		c = *kbdmop++;		/* fixes a bug in an old version  */
	else				/* where the quoted character was */
		c = tgetc();		/* not remembered.		  */
	if (kbdmip != NULL)
		*kbdmip++ = c;		/* if defining a macro, remember */
	if (n < 0)
		return FALSE;
	if (n == 0)
		return TRUE;
	if (c == '\n') {
		do {
			s = lnewline();
		} while (s == TRUE && --n);
		return s;
	}
	return linsert(n, c);
}

/*
 * Set tab size if given non-default argument (n <> 1).  Otherwise, insert a
 * tab into file.  If given argument, n, of zero, change to true tabs.
 * If n > 1, simulate tab stop every n-characters using spaces.
 * This has to be done in this slightly funny way because the
 * tab (in ASCII) has been turned into "C-I" (in 10
 * bit code) already. Bound to "C-I".
 */
int tab(int f, int n)
{
	if (n < 0)
		return FALSE;
	if (n == 0 || n > 1) {
		tabsize = n;
		return TRUE;
	}
	if (!tabsize)
		return linsert(1, '\t');
	return linsert(tabsize - (getccol() % tabsize), ' ');
}

/*
 * Open up some blank space. The basic plan
 * is to insert a bunch of newlines, and then back
 * up over them. Everything is done by the subcommand
 * procerssors. They even handle the looping. Normally
 * this is bound to "C-O".
 */
int openline(int f, int n)
{
	register int	i;
	register int	s;

	if (n < 0)
		return FALSE;
	if (n == 0)
		return TRUE;
	i = n;					/* Insert newlines.	*/
	do {
		s = lnewline();
	} while (s==TRUE && --i);
	if (s == TRUE)				/* Then back up overtop	*/
		s = backchar(f, n);		/* of them all.		*/
	return s;
}

/*
 * Insert a newline. Bound to "C-M".
 * If you are at the end of the line and the
 * next line is a blank line, just move into the
 * blank line. This makes "C-O" and "C-X C-O" work
 * nicely, and reduces the ammount of screen
 * update that has to be done. This would not be
 * as critical if screen update were a lot
 * more efficient.
 * (JFM: removed)
 */
int newline(int f, int n)
{
/*	register LINE	*lp; */
	register int	s;

	if (n < 0)
		return FALSE;
	while (n--) {
/*2.11*/
#if 0
		lp = curwp->w_dotp;
		if (llength(lp) == curwp->w_doto
		&& lp != curbp->b_linep
		&& llength(lforw(lp)) == 0) {
			if ((s = forwchar(FALSE, 1)) != TRUE)
				return s;
		} else
#endif
		if ((s = lnewline()) != TRUE)
			return s;
	}
	return TRUE;
}

/*
 * Delete blank lines around dot.
 * What this command does depends if dot is
 * sitting on a blank line. If dot is sitting on a
 * blank line, this command deletes all the blank lines
 * above and below the current line. If it is sitting
 * on a non blank line then it deletes all of the
 * blank lines after the line. Normally this command
 * is bound to "C-X C-O". Any argument is ignored.
 */
int deblank(int f, int n)
{
	register LINE	*lp1;
	register LINE	*lp2;
	register int	nld;
	WINDOW *lcurwp;

	lcurwp = curwp;
	lp1 = lcurwp->w_dotp;
	while (llength(lp1) == 0 && (lp2 = lback(lp1)) != curbp->b_linep)
		lp1 = lp2;
	lp2 = lp1;
	nld = 0;
	while ((lp2 = lforw(lp2)) != curbp->b_linep && llength(lp2) == 0)
		++nld;
	if (nld == 0)
		return TRUE;
	lcurwp->w_dotp = lforw(lp1);
	lcurwp->w_doto = 0;
	return ldelete(nld, TRUE);
}

/*
 * Insert a newline, then enough
 * tabs and spaces to duplicate the indentation
 * of the previous line. Assumes tabs are every eight
 * characters. Quite simple. Figure out the indentation
 * of the current line. Insert a newline by calling
 * the standard routine. Insert the indentation by
 * inserting the right number of tabs and spaces.
 * Return TRUE if all ok. Return FALSE if one
 * of the subcomands failed. Normally bound
 * to "C-J".
 */
int indent(int f, int n)
{
	register int	nicol;
	register int	c;
	register int	i;

	if (n < 0)
		return FALSE;
	while (n--) {
		nicol = 0;
		for (i = 0; i < llength(curwp->w_dotp); ++i) {
			c = lgetc(curwp->w_dotp, i);
			if (c != ' ' && c != '\t')
				break;
			if (c == '\t')
				nicol |= 0x07;
			++nicol;
		}
		if (lnewline() == FALSE
		 || ((i = nicol / 8) != 0 && linsert(i, '\t') == FALSE)
		 || ((i = nicol % 8) != 0 && linsert(i,  ' ') == FALSE))
			return FALSE;
	}
	return TRUE;
}

/*
 * Delete forward. This is real
 * easy, because the basic delete routine does
 * all of the work. Watches for negative arguments,
 * and does the right thing. If any argument is
 * present, it kills rather than deletes, to prevent
 * loss of text if typed with a big argument.
 * Normally bound to "C-D".
 */
int forwdel(int f, int n)
{
	LINE *curp;
	int curo, nn;

	if (n < 0)
		return backdel(f, -n);
	if (f != FALSE) {			/* Really a kill.	*/
		if ((lastflag & CFKILL) == 0)
			kdelete();
		thisflag |= CFKILL;
	}
	curp = curwp->w_dotp;
	curo = curwp->w_doto;
	nn = 0;
	while (n--) {
		nn++;
		if (curo == llength(curp)) {
			if (curp == curbp->b_linep)
				return FALSE;
			curp  = lforw(curp);
			curo  = 0;
		} else {
			curo++;
#ifdef UTF8
			if (curo == llength(curp))
				continue;
			while ((curp->l_text[curo] & 0xc0) == 0x80) {
				curo++;
				nn++;
				if (curo == llength(curp))
					break;
			}
#endif
		}
	}
	return ldelete(nn, f);
}

/*
 * Delete backwards. This is quite easy too,
 * because it's all done with other functions. Just
 * move the cursor back, and delete forwards.
 * Like delete forward, this actually does a kill
 * if presented with an argument. Bound to both
 * "RUBOUT" and "C-H".
 */
int backdel(int f, int n)
{
	register int	s;

	if (n < 0)
		return forwdel(f, -n);
#if 1
	if ((s = backchar(f, n)) == TRUE)
		s = forwdel(f, n);
#else
	if (f != FALSE) {			/* Really a kill.	*/
		if ((lastflag & CFKILL) == 0)
			kdelete();
		thisflag |= CFKILL;
	}
	if ((s = backchar(f, n)) == TRUE)
		s = ldelete(n, f);
#endif
	return s;
}

/*
 * Kill text. If called without an argument,
 * it kills from dot to the end of the line, unless it
 * is at the end of the line, when it kills the newline.
 * If called with an argument of 0, it kills from the
 * start of the line to dot. If called with a positive
 * argument, it kills from dot forward over that number
 * of newlines. If called with a negative argument it
 * kills backwards that number of newlines. Normally
 * bound to "C-K".
 */
int killtxt(int f, int n)
{
	register int	chunk;
	register LINE	*nextp;

	if ((lastflag & CFKILL) == 0)		/* Clear kill buffer if	*/
		kdelete();			/* last wasn't a kill.	*/
	thisflag |= CFKILL;
	if (f == FALSE) {
		chunk = llength(curwp->w_dotp)-curwp->w_doto;
		if (chunk == 0)
			chunk = 1;
	} else if (n == 0) {
		chunk = curwp->w_doto;
		curwp->w_doto = 0;
	} else if (n > 0) {
		chunk = llength(curwp->w_dotp) - curwp->w_doto + 1;
		nextp = lforw(curwp->w_dotp);
		while (--n) {
			if (nextp == curbp->b_linep)
				return FALSE;
			chunk += llength(nextp) + 1;
			nextp = lforw(nextp);
		}
	} else {
		mlwrite("neg kill");
		return FALSE;
	}
	return ldelete(chunk, TRUE);
}

/*
 * Yank text back from the kill buffer. This
 * is really easy. All of the work is done by the
 * standard insert routines. All you do is run the loop,
 * and check for errors. Bound to "C-Y". The blank
 * lines are inserted with a call to "newline"
 * instead of a call to "lnewline" so that the magic
 * stuff that happens when you type a carriage
 * return also happens when a carriage return is
 * yanked back from the kill buffer.
 * (JFM: removed)
 */
int yank(int f, int n)
{
	register int	c;
	register kill_t	i;

	if (n < 0)
		return FALSE;
	i = 0;
	while (n--) {
		i = 0;
		while ((c = kremove(i)) >= 0) {
			if (c == '\n') {
#if 1
				if (lnewline() == FALSE)
#else
				if (newline(FALSE, 1) == FALSE)
#endif
					return FALSE;
			} else {
				if (linsert(1, c) == FALSE)
					return FALSE;
			}
			++i;
		}
	}
	/* 2.14: if at EOB, remove the last newline */
	if (i > 0 && kremove(i - 1) == '\n'
	 && llength(curwp->w_dotp) == 0	/* 2.15 */
	 && lforw(curwp->w_dotp) == curbp->b_linep)
		ldelete(1, FALSE);
	return TRUE;
}

#if	GEM
/*
 * singlecase the characters in the given buffer. this is used for
 * buffer and file names on the ST, since everything becomes uppercase
 * (whether you like it or not) from the desktop and in file names.
 * this routine assumes ASCII.
 */
void fixname(bp)
register unsigned char *bp;
{
	register unsigned c;

	while (c = *bp)
#if	UPPERNM
		if (c >= 'a' && c <= 'z')
			*bp++ = c & ~0x20;	/* Lower to upper case */
#else
		if (c >= 'A' && c <= 'Z')
			*bp++ = c | 0x20;	/* Upper to lower case */
#endif
		else
			++bp;
}
#endif

int setfold(int f, int n)
{
	if (f)
		ffold = n;
	else
		ffold = !ffold;
	if (ffold)
		mlwrite("[Case fold on search]");
	else
		mlwrite("[Match case on search]");
	return TRUE;
}
