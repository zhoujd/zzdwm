/*
 * Buffer management.
 *
 * Some of the functions are internal and some are actually attached to user
 * keys.  Like everyone else, they set hints for the display system.
 *
 */
#include <stdio.h>
#include "ed.h"
#include <stdlib.h>
#include <string.h>

/*
 * Attach a buffer to a window.  The values of dot and mark come from the buffer
 * if the use count is 0. Otherwise, they come from some other window.
 */
int usebuffer(int f, int n)
{
	register BUFFER	*bp;
	register int	s;
	char		bufn[NBUFN];

	if ((s = mlreply("Use buffer: ", bufn, NBUFN)) != TRUE)
		return s;
#if	GEM
	fixname(bufn);
#endif
	if ((bp = bfind(bufn, TRUE, 0)) == NULL)
		return FALSE;
	showbuffer(bp, curwp, WFFORCE | WFHARD);
	curbp = bp;				/* Switch.		*/
	curwp->w_linep = bp->b_linep;		/* For macros, ignored.	*/
	return TRUE;
}

/*
 * Dispose of a buffer, by name.
 * Ask for the name. Look it up (don't get too upset if it isn't there at
 * all!).  Get quite upset if the buffer is the current buffer.  If it is
 * displayed, make sure the user wants it gone, and if (s)he does, delete
 * the windows associated with it as well.
 * Clear the buffer (ask if the buffer has been changed). Then free the header
 * line and the buffer header. Bound to "C-X K".
 */
int killbuffer(int f, int n)
{
	register BUFFER	*bp;
	register BUFFER	*bp1;
	register BUFFER	*bp2;
	register int	s;
	char		bufn[NBUFN];

	if ((s = mlreply("Kill buffer: ", bufn, NBUFN)) == ABORT)
		return s;
#if	GEM
	fixname(bufn);
#endif
	if (s == TRUE) {
		if ((bp = bfind(bufn, FALSE, 0)) == NULL) /* Easy if unknown.	*/
			return TRUE;
	} else {
		bp = curbp;
	}

#if 1
	/* search the previous and next buffers */
	if (bp == bheadp) {
		bp1 = NULL;
	} else {
		bp1 = bheadp;
		while (bp1->b_bufp != bp)
			bp1 = bp1->b_bufp;
	}
	if ((bp2 = bp->b_bufp) == NULL) {
		if (!bp1) {
			mlwrite("Only buffer");
			return FALSE;
		}
	}
#else
	if (bp->b_nwnd != 0) {			/* If on screen.	*/
		if (curbp == bp) {
			mlwrite("Buffer is current buffer");
			return FALSE;
		} else {
			if ((s = mlyesno("Buffer is displayed: Really kill"))
					!= TRUE)
				return FALSE;
		}
	}
#endif
	if ((s = bclear(bp)) != TRUE)		/* Blow text away.	*/
		return s;
#if 1
	/* replace the content of windows */
	if (bp->b_nwnd != 0) {			/* If on screen.	*/
		WINDOW *wp;
		BUFFER *bp3;

		bp3 = bp2 ? bp2 : bp1;
		for (wp = wheadp; bp->b_nwnd > 0; wp = wp->w_wndp) {
			if (wp->w_bufp == bp)
				showbuffer(bp3, wp, WFFORCE | WFHARD);
		}
		if (curbp == bp)
			curbp = bp3;
	}
#else
	while (bp->b_nwnd != 0)			/* Blow windows away.	*/
		zapbwind(bp);
#endif
	free((char *) bp->b_linep);		/* Release header line.	*/
#if 0
	bp1 = NULL;				/* Find the header.	*/
	bp2 = bheadp;
	while (bp2 != bp) {
		bp1 = bp2;
		bp2 = bp2->b_bufp;
	}
	bp2 = bp2->b_bufp;			/* Next one in chain.	*/
#endif
	if (!bp1)				/* Unlink it.		*/
		bheadp = bp2;
	else
		bp1->b_bufp = bp2;
	free((char *) bp);			/* Release buffer block	*/
	return TRUE;
}

/*
 * List all of the active buffers. First update the special buffer that
 * holds the list. Next make sure at least 1 window is displaying the
 * buffer list, splitting the screen if this is what it takes.
 * Lastly, repaint all of the windows that are displaying the list.
 * Bound to "C-X C-B".
 */
int listbuffers(int f, int n)
{
	register WINDOW	*wp;
	register BUFFER	*bp;
	register int	s;

	if ((s = makelist()) != TRUE)
		return s;
	if (blistp->b_nwnd == 0) {		/* Not on screen yet.	*/
		if ((wp = wpopup()) == NULL)
			return FALSE;
		bp = wp->w_bufp;
		if (--bp->b_nwnd == 0) {
			bp->b_dotp  = wp->w_dotp;
			bp->b_doto  = wp->w_doto;
			bp->b_markp = wp->w_markp;
			bp->b_marko = wp->w_marko;
		}
		wp->w_bufp  = blistp;
		++blistp->b_nwnd;
	}
	wp = wheadp;
	while (wp) {
		if (wp->w_bufp == blistp) {
			wp->w_linep = lforw(blistp->b_linep);
			wp->w_dotp  = lforw(blistp->b_linep);
			wp->w_doto  = 0;
			wp->w_markp = NULL;
			wp->w_marko = 0;
			wp->w_flag |= WFMODE|WFHARD;
		}
		wp = wp->w_wndp;
	}
	return TRUE;
}

#if 0
static void ltoa_(char *buf, int width, long num)
{
	buf[width] = 0;				/* End of string.	*/
	while (num >= 10) {			/* Conditional digits.	*/
		buf[--width] = (num % 10) + '0';
		num /= 10;
	}
	buf[--width] = num + '0';		/* Always 1 digit.	*/
#if 0
	while (width > 0)			/* Pad with blanks.	*/
		buf[--width] = ' ';
#endif
}
#endif

/*
 * This routine rebuilds the text in the special secret buffer that holds
 * the buffer list. It is called by the list buffers command. Return TRUE
 * if everything works. Return FALSE if there is an error
 * (if there is no memory).
 */
int makelist(void)
{
/*	register char	*cp1; */
	register char	*cp2;
	register BUFFER	*bp;
	register LINE	*lp;
	register long	nlines;
	register long	nbytes;
	register int	c;
/*	char		b[6 + 1]; */
	char		line[128];

	blistp->b_flag &= ~BFCHG;		/* Don't complain!	*/
	if ((c = bclear(blistp)) != TRUE)	/* Blow old text away	*/
		return c;
	blistp->b_fname[0] = '\0';
	if (addline(blistp, "C   Size  Lines Buffer           File") == FALSE
	 || addline(blistp, "-   ----  ----- ------           ----") == FALSE)
		return FALSE;
	for (bp = bheadp; bp != NULL; bp = bp->b_bufp) {
		if (bp->b_flag & BFTEMP)	/* Skip magic ones.	*/
			continue;
#if 1
#if 1
		nbytes = 0;			/* Count bytes in buf.	*/
		nlines = 0;			/* Count lines in buf.	*/
		lp = lforw(bp->b_linep);
		while (lp != bp->b_linep) {
			nlines++;
			nbytes += llength(lp) + 1;
			lp = lforw(lp);
		}
		cp2 = &bp->b_fname[0];		/* File name		*/
		if (!*cp2)
			snprintf(line, sizeof line, "%c %6ld %6ld %-*s",
				(bp->b_flag & BFCHG) ? '*' : ' ',
				nbytes, nlines, NBUFN, bp->b_bname);
		else
			snprintf(line, sizeof line, "%c %6ld %6ld %-*s %s",
				(bp->b_flag & BFCHG) ? '*' : ' ',
				nbytes, nlines, NBUFN, bp->b_bname, cp2);
#else
		cp1 = &line[0];			/* Start at left edge	*/
		memset(line, ' ', sizeof line);
		if (bp->b_flag & BFCHG)		/* "*" if changed	*/
			*cp1 = '*';
		nbytes = 0;			/* Count bytes in buf.	*/
		nlines = 0;			/* Count lines in buf.	*/
		lp = lforw(bp->b_linep);
		while (lp != bp->b_linep) {
			nlines++;
			nbytes += llength(lp)+1;
			lp = lforw(lp);
		}
		ltoa_(cp1 + 2, 6, nbytes);	/* 6 digit buffer size.	*/
		ltoa_(cp1 + 9, 6, nlines);	/* 6 digit # of lines	*/
		cp2 = bp->b_bname;		/* Buffer name		*/
		nbytes = strlen(cp2);
		memcpy(cp1 + 16, cp2, nbytes);
		cp2 = &bp->b_fname[0];		/* File name		*/
		if (*cp2 == 0) {
			cp1[16 + nbytes] = '\0';
		} else {
			nbytes = strlen(cp2);
			if (nbytes + 33 > sizeof line - 1)
				nbytes = sizeof line - 33 - 1;
			memcpy(cp1 + 33, cp2, nbytes);
			cp1[33 + nbytes] = '\0';
		}
#endif
#else
		cp1 = &line[0];			/* Start at left edge	*/
		if ((bp->b_flag & BFCHG) != 0)	/* "*" if changed	*/
			*cp1++ = '*';
		else	*cp1++ = ' ';
		*cp1++ = ' ';			/* Gap.			*/
		nbytes = 0;			/* Count bytes in buf.	*/
		nlines = 0;			/* Count lines in buf.	*/
		lp = lforw(bp->b_linep);
		while (lp != bp->b_linep) {
			nlines++;
			nbytes += llength(lp)+1;
			lp = lforw(lp);
		}
		ltoa_(b, 6, nbytes);		/* 6 digit buffer size.	*/
		cp2 = &b[0];
		while ((c = *cp2++) != 0)
			*cp1++ = c;
		*cp1++ = ' ';			/* Gap.			*/
		ltoa_(b, 6, nlines);		/* 6 digit # of lines	*/
		cp2 = &b[0];
		while ((c = *cp2++) != 0)
			*cp1++ = c;
		*cp1++ = ' ';			/* Gap.			*/
		cp2 = &bp->b_bname[0];		/* Buffer name		*/
		while ((c = *cp2++) != 0)
			*cp1++ = c;
		cp2 = &bp->b_fname[0];		/* File name		*/
		if (*cp2 != 0) {
			while (cp1 < &line[1 + 1 + 6 + 1 + 6 + 1 + NBUFN + 1])
				*cp1++ = ' ';		
			while ((c = *cp2++) != 0) {
				if (cp1 < &line[128-1])
					*cp1++ = c;
			}
		}
		*cp1 = 0;			/* Add to the buffer.	*/
#endif
		if (addline(blistp, line) == FALSE)
			return FALSE;
	}
	return TRUE;				/* All done		*/
}

/*
 * The argument "text" points to a string. Append this line to the buffer
 * specified. Handcraft the EOL on the end. Return TRUE if it worked and
 * FALSE if you ran out of room.
 */
int addline(BUFFER *bp, char *text)
{
	register LINE	*lp;
//	register int	i;
	register int	ntext;

	ntext = strlen(text);
	if ((lp = lalloc(ntext)) == NULL)
		return FALSE;
//	for (i = 0; i < ntext; ++i)
//		lputc(lp, i, text[i]);
	memcpy(lp->l_text, text, ntext);
	bp->b_linep->l_bp->l_fp = lp;		/* Hook onto the end	*/
	lp->l_bp = bp->b_linep->l_bp;
	bp->b_linep->l_bp = lp;
	lp->l_fp = bp->b_linep;
	if (bp->b_dotp == bp->b_linep)		/* If "." is at the end	*/
		bp->b_dotp = lp;		/* move it to new line	*/
	return TRUE;
}

/*
 * Look through the list of buffers. Return TRUE if there are any changed
 * buffers. Buffers that hold magic internal stuff are not considered;
 * who cares if the list of buffer names is hacked.
 * Return FALSE if no buffers have been changed.
 */
int anycb(void)
{
	register BUFFER	*bp;

	bp = bheadp;
	while (bp) {
		if (!(bp->b_flag & (BFTEMP | BFNOWRT | BFERROR))
		 && (bp->b_flag & BFCHG))
			return TRUE;
		bp = bp->b_bufp;
	}
	return FALSE;
}

/*
 * Find a buffer, by name. Return a pointer to the BUFFER structure
 * associated with it. If the named buffer is found, but is a TEMP buffer
 * (like the buffer list) conplain. If the buffer is not found and the "cflag"
 * is TRUE, create it. The "bflag" is the settings for the flags in in buffer.
 */
BUFFER *bfind(char *bname, int cflag, int bflag)
{
	register BUFFER	*bp;
	register LINE	*lp;

	bp = bheadp;
	while (bp) {
		if (strcmp(bname, bp->b_bname) == 0) {
			if ((bp->b_flag & BFTEMP) != 0) {
				mlwrite("Cannot select builtin buffer");
				return NULL;
			}
			return bp;
		}
		bp = bp->b_bufp;
	}
	if (cflag != FALSE) {
		if ((bp = (BUFFER *) malloc(sizeof(BUFFER))) == NULL)
			return NULL;
		if ((lp = lalloc(0)) == NULL) {
			free((char *) bp);
			return NULL;
		}
		bp->b_bufp  = bheadp;
		bheadp = bp;
		bp->b_dotp  = lp;
		bp->b_doto  = 0;
		bp->b_markp = NULL;
		bp->b_marko = 0;
		bp->b_flag  = bflag;
		bp->b_nwnd  = 0;
		bp->b_linep = lp;
//		strcpy(bp->b_fname, "");
		bp->b_fname[0] = '\0';
		strcpy(bp->b_bname, bname);
		lp->l_fp = lp;
		lp->l_bp = lp;
	}
	return bp;
}

/*
 * This routine blows away all of the text in a buffer. If the buffer is
 * marked as changed then we ask if it is ok to blow it away; this is
 * to save the user the grief of losing text. The window chain is nearly
 * always wrong if this gets called; the caller must arrange for the updates
 * that are required. Return TRUE if everything looks good.
 */
int bclear(BUFFER *bp)
{
	register LINE	*lp;
	register int	s;
	
	if (!(bp->b_flag & BFTEMP)		/* Not scratch buffer.	*/
	 && (bp->b_flag & BFCHG)		/* Something changed	*/
	 && (s = mlyesno("Discard changes")) != TRUE)
		return s;
	bp->b_flag  &= ~(BFCHG | BFFLCK);	/* Not changed		*/
	while ((lp = lforw(bp->b_linep)) != bp->b_linep)
		lfree(lp);
	bp->b_dotp  = bp->b_linep;		/* Fix "."		*/
	bp->b_doto  = 0;
	bp->b_markp = NULL;			/* Invalidate "mark"	*/
	bp->b_marko = 0;
	return TRUE;
}

/*
 * Display the given buffer in the given window. Flags indicated
 * action on redisplay.
 */
int showbuffer(BUFFER *bp, WINDOW *wp, int flags)
{
	BUFFER *obp;
	WINDOW *owp;

	obp = wp->w_bufp;

	/* First, detach the old buffer from the window */
	if (--curbp->b_nwnd == 0) {		/* last use */
		obp->b_dotp  = wp->w_dotp;
		obp->b_doto  = wp->w_doto;
		obp->b_markp = wp->w_markp;
		obp->b_marko = wp->w_marko;
	}

	/* Now, attach the new buffer to the window */
	wp->w_bufp = bp;
	wp->w_flag |= WFMODE | flags;

	if (bp->b_nwnd++ == 0) {		/* first use */
		wp->w_dotp  = bp->b_dotp;
		wp->w_doto  = bp->b_doto;
		wp->w_markp = bp->b_markp;
		wp->w_marko = bp->b_marko;
		wp->w_shift = 0;		/* JFM : no shift */
	} else {

		/* already on screen, steal values from other window */
		for (owp = wheadp; owp; owp = owp->w_wndp) {
			if (owp != wp && owp->w_bufp == bp) {
				wp->w_dotp  = owp->w_dotp;
				wp->w_doto  = owp->w_doto;
				wp->w_markp = owp->w_markp;
				wp->w_marko = owp->w_marko;
				wp->w_shift = owp->w_shift;	/* JFM */
				break;
			}
		}
	}
	return TRUE;
}

#if	LIBHELP
/*
 * Put up a help window with the text pointed to by txt.
 */
int helpwindow(char *tag)
{
	register WINDOW	*wp;
	register BUFFER	*bp;
	register int	s;

	if ((s = makehelp(tag)) != TRUE) {
		while (helpbp->b_nwnd != 0)	/* If on screen		*/
			zaphelp(0, 0);		/* Delete windows	*/
		return s;
	}
	if (helpbp->b_nwnd == 0) {		/* Not on screen yet.	*/
		if ((wp = wpopup(helpbp)) == NULL)
			return FALSE;
		bp = wp->w_bufp;
		if (--bp->b_nwnd == 0) {
			bp->b_dotp  = wp->w_dotp;
			bp->b_doto  = wp->w_doto;
			bp->b_markp = wp->w_markp;
			bp->b_marko = wp->w_marko;
		}
		wp->w_bufp  = helpbp;
		++helpbp->b_nwnd;
	}
	wp = wheadp;
	while (wp != NULL) {
		if (wp->w_bufp == helpbp) {
			wp->w_linep = lforw(helpbp->b_linep);
			wp->w_dotp  = lforw(helpbp->b_linep);
			wp->w_doto  = 0;
			wp->w_markp = NULL;
			wp->w_marko = 0;
			wp->w_flag |= WFMODE|WFHARD;
		}
		wp = wp->w_wndp;
	}
	return TRUE;
}

/*
 * Service routine for help...
 */
void addhelp(char *str)
{
	addline(helpbp, str);
}

/*
 * This routine rebuilds the text in the special secret buffer that holds
 * the help.  It is called by the help command.  Return TRUE if
 * everything works.  Return FALSE if there is an error (if there is no memory
 * or no help for the topic or file not found.)
 */
int makehelp(char *tag)
{
	extern char *helpfile;
	register int c;

	helpbp->b_flag &= ~BFCHG;		/* Don't complain!	*/
	if ((c = bclear(helpbp)) != TRUE)	/* Blow old text away	*/
		return c;
	strcpy(helpbp->b_fname, tag);

	if ((c = help(tag, addhelp)) < 0)
		mlwrite("[Can't open help file %s]", helpfile);
	else if (c > 0)
		mlwrite("[No help available for topic \"%s\"]", tag);

	return c == 0;			/* All done		*/
}

/*
 * Put up a help window with an index of matching topics
 */
int topicwindow(char *tag)
{
	register WINDOW	*wp;
	register BUFFER	*bp;
	register int	s;

	if ((s = makeindex(tag)) != TRUE) {
		while (helpbp->b_nwnd != 0)	/* If on screen		*/
			zaphelp(0, 0);		/* Delete windows	*/
		return s;
	}
	if (helpbp->b_nwnd == 0) {		/* Not on screen yet.	*/
		if ((wp = wpopup(helpbp)) == NULL)
			return FALSE;
		bp = wp->w_bufp;
		if (--bp->b_nwnd == 0) {
			bp->b_dotp  = wp->w_dotp;
			bp->b_doto  = wp->w_doto;
			bp->b_markp = wp->w_markp;
			bp->b_marko = wp->w_marko;
		}
		wp->w_bufp  = helpbp;
		++helpbp->b_nwnd;
	}
	wp = wheadp;
	while (wp != NULL) {
		if (wp->w_bufp == helpbp) {
			wp->w_linep = lforw(helpbp->b_linep);
			wp->w_dotp  = lforw(helpbp->b_linep);
			wp->w_doto  = 0;
			wp->w_markp = NULL;
			wp->w_marko = 0;
			wp->w_flag |= WFMODE|WFHARD;
		}
		wp = wp->w_wndp;
	}
	return TRUE;
}

/*
 * This routine rebuilds the text in the special secret buffer that holds
 * the help.  It is called by the help command.  Return TRUE if
 * everything works.  Return FALSE if there is an error (if there is no memory
 * or no help for the topic or file not found.)
 */
int makeindex(char *tag)
{
	extern char *helpfile;
	register int c;

	helpbp->b_flag &= ~BFCHG;		/* Don't complain!	*/
	if ((c = bclear(helpbp)) != TRUE)	/* Blow old text away	*/
		return c;
	strcpy(helpbp->b_fname, "index for ");
	strcat(helpbp->b_fname, tag);

	if ((c = indexhelp(tag, addhelp)) < 0)
		mlwrite("[Can't open help file %s]", helpfile);
	else if (c > 0)
		mlwrite("[No entries matching \"%s\"]", tag);

	return c == 0;			/* All done		*/
}
#endif
