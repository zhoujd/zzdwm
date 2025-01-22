/*
 * The functions in this file are a general set of line management utilities.
 * They are the only routines that touch the text. They also touch the buffer
 * and window structures, to make sure that the necessary updating gets done.
 * There are routines in this file that handle the kill buffer too.
 * It isn't here for any good reason.
 *
 * Note that this code only updates the dot and mark values in the window list.
 * Since all the code acts on the current window, the buffer that we are
 * editing must be being displayed, which means that "b_nwnd" is non zero,
 * which means that the dot and mark values in the buffer headers are nonsense.
 */
#include	<stdio.h>
#include	"ed.h"
#include	<stdlib.h>

#define NBLOCK 16			/* Line block chunk size	*/
#define	KBLOCK	256			/* Kill buffer block size	*/

static char *kbufp = NULL;		/* Kill buffer data		*/
static kill_t kused = 0;		/* # of bytes used in KB	*/
static kill_t ksize = 0;		/* # of bytes allocated in KB	*/

static int ldelnewline(void);

/*
 * This routine allocates a block of memory large enough to hold a LINE
 * containing "used" characters. The block is always rounded up a bit.
 * Return a pointer to the new block, or NULL if there isn't any memory left.
 * Print a message in the message line if no space.
 */
LINE *lalloc(register int used)
{
	register LINE	*lp;
	register int	size;

	size = (used + NBLOCK - 1) & ~(NBLOCK - 1);
	if (size == 0)				/* Assume that an empty	*/
		size = NBLOCK;			/* line is for type-in.	*/
	if ((lp = (LINE *) malloc(sizeof(LINE) + size)) == NULL) {
		mlwrite("Cannot allocate %d bytes", size);
		return NULL;
	}
	lp->l_size = size;
	lp->l_used = used;
#if OLD_NUMBER
	lp->l_lnumber = 0;			/* This is a new line... */
#endif
#ifdef COLOR
	lp->anchor = 0;
#endif
#ifdef MMAP
	lp->l_text = (unsigned char *) (lp + 1);
#endif
	return lp;
}

/*
 * Delete line "lp". Fix all of the links that might point at it (they are
 * moved to offset 0 of the next line. Unlink the line from whatever buffer it
 * might be in. Release the memory. The buffers are updated too; the magic
 * conditions described in the above comments don't hold here.
 */
void lfree(LINE *lp)
{
	BUFFER	*bp;
	WINDOW	*wp;

	for (wp = wheadp;wp ; wp = wp->w_wndp) {
		if (wp->w_linep == lp)
			wp->w_linep = lp->l_fp;
		if (wp->w_dotp  == lp) {
			wp->w_dotp  = lp->l_fp;
			wp->w_doto  = 0;
		}
		if (wp->w_markp == lp) {
			wp->w_markp = lp->l_fp;
			wp->w_marko = 0;
		}
	}
	for (bp = bheadp; bp; bp = bp->b_bufp) {
		if (bp->b_nwnd == 0) {
			if (bp->b_dotp  == lp) {
				bp->b_dotp = lp->l_fp;
				bp->b_doto = 0;
			}
			if (bp->b_markp == lp) {
				bp->b_markp = lp->l_fp;
				bp->b_marko = 0;
			}
		}
	}
	lp->l_bp->l_fp = lp->l_fp;
	lp->l_fp->l_bp = lp->l_bp;
	free(lp);
}

/*
 * This routine gets called when a character is changed in place in the
 * current buffer. It updates all of the required flags in the buffer and
 * window system. The flag used is passed as an argument; if the buffer is
 * being displayed in more than 1 window we change EDIT to HARD.
 * Set MODE if the mode line needs to be updated (the "*" has to be set).
 */
int lchange(int flag)
{
	WINDOW	*wp;

/*2.17*/
	/* check if the buffer is readonly */
	if (curbp->b_flag & (BFNOWRT | BFTEMP)) { /* if read only */
		if (curbp == blistp) {		/* if buffer list */
			BUFFER *bp;
			LINE *lp;
			unsigned char *cp1, *cp2;

			lp = curwp->w_dotp;	/* Current line		*/
			cp1 = lp->l_text;
			cp1 += 16;		/* buffer name */
			cp2 = cp1;
			while (*cp2 != ' ')
				cp2++;
			*cp2 = '\0';
			bp = bfind((char *) cp1, FALSE, FALSE);
			*cp2 = ' ';
			if (bp) {
				showbuffer(bp, curwp, WFFORCE | WFHARD);
				curbp = bp;
				curwp->w_linep = bp->b_linep;
				return FALSE;
			}
		}
		mlwrite("Readonly buffer");
		return FALSE;
	}

	if (curbp->b_nwnd != 1)			/* Ensure hard.		*/
		flag = WFHARD;
	if (!(curbp->b_flag & BFCHG)) {		/* First change, so 	*/
		flag |= WFMODE;			/* update mode lines.	*/
		curbp->b_flag |= BFCHG;
	}
	for (wp = wheadp; wp; wp = wp->w_wndp) {
		if (wp->w_bufp == curbp)
			wp->w_flag |= flag;
	}
	return TRUE;
}

/*
 * Insert "n" copies of the character "c" at the current location of dot.
 * In the easy case all that happens is the text is stored in the line.
 * In the hard case, the line has to be reallocated.
 * When the window list is updated, take special care; I screwed it up once.
 * You always update dot in the current window. You update mark, and a
 * dot in another window, if it is greater than the place where you did
 * the insert.
 * Return TRUE if all is well, and FALSE on errors.
 */
int linsert(int n, int c)
{
	unsigned char *cp1, *cp2;
	LINE	*lp1;
	LINE	*lp2;
	LINE	*lp3;
	int	doto;
	int	i;
	WINDOW	*wp;

	if (!lchange(WFEDIT))
		return FALSE;
	lp1 = curwp->w_dotp;			/* Current line		*/
	if (lp1 == curbp->b_linep) {		/* At the end: special	*/
#if 0
		if (curwp->w_doto != 0) {
			mlwrite("bug: linsert");
			return FALSE;
		}
#endif
		if ((lp2 = lalloc(n)) == NULL)	/* Allocate new line	*/
			return FALSE;
		lp3 = lp1->l_bp;		/* Previous line	*/
		lp3->l_fp = lp2;		/* Link in		*/
		lp2->l_fp = lp1;
		lp1->l_bp = lp2;
		lp2->l_bp = lp3;
		for (i = 0; i < n; ++i)
			lp2->l_text[i] = c;
		curwp->w_dotp = lp2;
		curwp->w_doto = n;
#ifdef COLOR
		lp2->anchor = lp1->anchor;
#endif
		return TRUE;
	}
	doto = curwp->w_doto;			/* Save for later.	*/
	if (lp1->l_used + n > lp1->l_size) {	/* Hard: reallocate	*/
		if ((lp2 = lalloc(lp1->l_used + n)) == NULL)
			return FALSE;
		cp1 = &lp1->l_text[0];
		cp2 = &lp2->l_text[0];
		while (cp1 != &lp1->l_text[doto])
			*cp2++ = *cp1++;
		cp2 += n;
		while (cp1 != &lp1->l_text[lp1->l_used])
			*cp2++ = *cp1++;
		lp1->l_bp->l_fp = lp2;
		lp2->l_fp = lp1->l_fp;
		lp1->l_fp->l_bp = lp2;
		lp2->l_bp = lp1->l_bp;
#ifdef COLOR
		lp2->anchor = lp1->anchor;
#endif
		free(lp1);
	} else {				/* Easy: in place	*/
		lp2 = lp1;			/* Pretend new line	*/
		lp2->l_used += n;
		cp2 = &lp1->l_text[lp1->l_used];
		cp1 = cp2 - n;
		while (cp1 != &lp1->l_text[doto])
			*--cp2 = *--cp1;
	}
	for (i = 0; i < n; ++i)			/* Add the characters	*/
		lp2->l_text[doto + i] = c;
	for (wp = wheadp; wp; wp = wp->w_wndp) { /* Update windows	*/
		if (wp->w_linep == lp1)
			wp->w_linep = lp2;
		if (wp->w_dotp == lp1) {
			wp->w_dotp = lp2;
			if (wp == curwp || wp->w_doto > doto)
				wp->w_doto += n;
		}
		if (wp->w_markp == lp1) {
			wp->w_markp = lp2;
			if (wp->w_marko > doto)
				wp->w_marko += n;
		}
	}
	return TRUE;
}

/*
 * Insert a newline into the buffer at the current location of dot in the
 * current window. The funny ass-backwards way it does things is not a botch;
 * it just makes the last line in the file not a special case. Return TRUE if
 * everything works out and FALSE on error (memory allocation failure).
 * The update of dot and mark is a bit easier then in the above case, because
 * the split forces more updating.
 */
int lnewline(void)
{
	unsigned char *cp1, *cp2;
	LINE	*lp1;
	LINE	*lp2;
	int	doto;
	WINDOW	*wp;

	if (!lchange(WFHARD))
		return FALSE;
	lp1  = curwp->w_dotp;			/* Get the address and	*/
	doto = curwp->w_doto;			/* offset of "."	*/
	if ((lp2 = lalloc(doto)) == NULL)	/* New first half line	*/
		return FALSE;
	cp1 = &lp1->l_text[0];			/* Shuffle text around	*/
	cp2 = &lp2->l_text[0];
	while (cp1 != &lp1->l_text[doto])
		*cp2++ = *cp1++;
	cp2 = &lp1->l_text[0];
	while (cp1 != &lp1->l_text[lp1->l_used])
		*cp2++ = *cp1++;
	lp1->l_used -= doto;
	lp2->l_bp = lp1->l_bp;
	lp1->l_bp = lp2;
	lp2->l_bp->l_fp = lp2;
	lp2->l_fp = lp1;
#ifdef COLOR
	lp2->anchor = lp1->anchor;
#endif
	for (wp = wheadp; wp; wp = wp->w_wndp) { /* Windows		*/
		if (wp->w_linep == lp1)
			wp->w_linep = lp2;
		if (wp->w_dotp == lp1) {
			if (wp->w_doto < doto)
				wp->w_dotp = lp2;
			else
				wp->w_doto -= doto;
		}
		if (wp->w_markp == lp1) {
			if (wp->w_marko < doto)
				wp->w_markp = lp2;
			else
				wp->w_marko -= doto;
		}
	}	
	return TRUE;
}

/*
 * This function deletes "n" bytes, starting at dot. It understands how to deal
 * with end of lines, etc. It returns TRUE if all of the characters were
 * deleted, and FALSE if they were not (because dot ran into the end of
 * the buffer. The "kflag" is TRUE if the text should be put in the kill buffer.
 */
int ldelete(int n, int kflag)
{
	unsigned char *cp1, *cp2;
	LINE	*dotp;
	int	doto;
	int	chunk;
	WINDOW	*wp;

	if (!lchange(WFEDIT))
		return FALSE;
	while (n != 0) {
		dotp = curwp->w_dotp;
		doto = curwp->w_doto;
		if (dotp == curbp->b_linep)	/* Hit end of buffer.	*/
			return FALSE;
		chunk = dotp->l_used - doto;	/* Size of chunk.	*/
		if (chunk == 0) {		/* End of line, merge.	*/
			lchange(WFHARD);
			if (ldelnewline() == FALSE
			 || (kflag != FALSE && kinsert('\n') == FALSE))
				return FALSE;
			--n;
			continue;
		}
/*		lchange(WFEDIT); */
		cp1 = &dotp->l_text[doto];	/* Scrunch text.	*/
		if (chunk > n)
			chunk = n;
		n -= chunk;
		cp2 = cp1 + chunk;
		if (kflag != FALSE) {		/* Kill?		*/
			while (cp1 != cp2) {
				if (kinsert(*cp1) == FALSE)
					return FALSE;
				++cp1;
			}
			cp1 = &dotp->l_text[doto];
		}
		while (cp2 != &dotp->l_text[dotp->l_used])
			*cp1++ = *cp2++;
		dotp->l_used -= chunk;
		for (wp = wheadp; wp; wp = wp->w_wndp) { /* Fix windows		*/
			if (wp->w_dotp == dotp && wp->w_doto > doto) {
				wp->w_doto -= chunk;
				if (wp->w_doto < doto)
					wp->w_doto = doto;
			}	
			if (wp->w_markp == dotp && wp->w_marko > doto) {
				wp->w_marko -= chunk;
				if (wp->w_marko < doto)
					wp->w_marko = doto;
			}
		}
	}
	return TRUE;
}

/*
 * Delete a newline. Join the current line with the next line. If the next
 * line is the magic header line always return TRUE; merging the last line
 * with the header line can be thought of as always being a successful
 * operation, even if nothing is done, and this makes the kill buffer
 * work "right". Easy cases can be done by shuffling data around.
 * Hard cases require that lines be moved about in memory.
 * Return FALSE on error and TRUE if all looks ok. Called by "ldelete" only.
 */
static int ldelnewline(void)
{
	unsigned char *cp1, *cp2;
	LINE	*lp1;
	LINE	*lp2;
	LINE	*lp3;
	WINDOW	*wp;

	lp1 = curwp->w_dotp;			/* This line...		*/
	lp2 = lp1->l_fp;			/* The next line...	*/

	if (lp2 == curbp->b_linep) {		/* At the buffer end.	*/
		if (lp1->l_used == 0)		/* Blank line.		*/
			lfree(lp1);
		return TRUE;
	}
#if OLD_NUMBER
	/* Fix file-line numbers.		*/
	if ((lp1->l_used == 0) || (lp1->l_lnumber == 0))
		lp1->l_lnumber = lp2->l_lnumber;
#endif

	if (lp2->l_used <= lp1->l_size - lp1->l_used) {
		cp1 = &lp1->l_text[lp1->l_used];
		cp2 = &lp2->l_text[0];
		while (cp2 != &lp2->l_text[lp2->l_used])
			*cp1++ = *cp2++;
		wp = wheadp;
		while (wp != NULL) {
			if (wp->w_linep == lp2)
				wp->w_linep = lp1;
			if (wp->w_dotp == lp2) {
				wp->w_dotp  = lp1;
				wp->w_doto += lp1->l_used;
			}
			if (wp->w_markp == lp2) {
				wp->w_markp  = lp1;
				wp->w_marko += lp1->l_used;
			}
			wp = wp->w_wndp;
		}		
		lp1->l_used += lp2->l_used;
		lp1->l_fp = lp2->l_fp;
		lp2->l_fp->l_bp = lp1;
		free(lp2);
		return TRUE;
	}
	if ((lp3 = lalloc(lp1->l_used + lp2->l_used)) == NULL)
		return FALSE;
	cp1 = &lp1->l_text[0];
	cp2 = &lp3->l_text[0];
	while (cp1 != &lp1->l_text[lp1->l_used])
		*cp2++ = *cp1++;
	cp1 = &lp2->l_text[0];
	while (cp1 != &lp2->l_text[lp2->l_used])
		*cp2++ = *cp1++;
	lp1->l_bp->l_fp = lp3;
	lp3->l_fp = lp2->l_fp;
	lp2->l_fp->l_bp = lp3;
	lp3->l_bp = lp1->l_bp;
	for (wp = wheadp; wp; wp = wp->w_wndp) {
		if (wp->w_linep == lp1 || wp->w_linep == lp2)
			wp->w_linep = lp3;
		if (wp->w_dotp == lp1)
			wp->w_dotp  = lp3;
		else if (wp->w_dotp == lp2) {
			wp->w_dotp  = lp3;
			wp->w_doto += lp1->l_used;
		}
		if (wp->w_markp == lp1)
			wp->w_markp  = lp3;
		else if (wp->w_markp == lp2) {
			wp->w_markp  = lp3;
			wp->w_marko += lp1->l_used;
		}
	}
	free(lp1);
	free(lp2);
	return TRUE;
}

/*
 * Delete all of the text saved in the kill buffer.  Called by commands when
 * a new kill context is being created. The kill buffer array is released,
 * just in case the buffer has grown to immense size. No errors.
 */
int kdelete(void)
{
	if (kbufp) {
		free((char *) kbufp);
		kbufp = NULL;
		kused = 0;
		ksize = 0;
	}
	return 1;
}

/*
 * Insert a character to the kill buffer, enlarging the buffer if there
 * isn't any room. Always grow the buffer in chunks, on the assumption
 * that if you put something in the kill buffer you are going to put more
 * stuff there too later. Return TRUE if all is well, and FALSE on errors.
 */
int kinsert(int c)
{
	char	*nbufp;
#if	GEM
	extern char *kbrealloc();
#else
	kill_t	i;
#endif

	if (kused == ksize) {
#if	GEM
		/* Note -- This depends of realloc() still working when */
		/* passed a NULL pointer.  This is true of the ANSI */
		/* version, and the ST library version for 3.0 and later. */
		if ((nbufp = kbrealloc(kbufp, (kill_t) (ksize + KBLOCK))) == NULL)
			return FALSE;
		kbufp = nbufp;
#else
		if ((nbufp = malloc(ksize + KBLOCK)) == NULL)
			return FALSE;
		for (i = 0; i < ksize; ++i)
			nbufp[i] = kbufp[i];
		if (kbufp != NULL)
			free((char *) kbufp);
#endif
		kbufp = nbufp;
		ksize += KBLOCK;
	}
	kbufp[kused++] = c;
	return TRUE;
}

/*
 * This function gets characters from the kill buffer. If the character index
 * "n" is off the end, it returns "-1". This lets the caller just scan along
 * until it gets a "-1" back.
 */
int kremove(kill_t n)
{
	if (n >= kused)
		return -1;
	return kbufp[n] & 0xff;
}
