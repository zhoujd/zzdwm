/*
 * The routines in this file
 * implement commands that work word at
 * a time. There are all sorts of word mode
 * commands. If I do any sentence and/or paragraph
 * mode commands, they are likely to be put in
 * this file.
 * JFM: reset kill buffer when word delete
 */
#include	<stdio.h>
#include	"ed.h"

static int backwhite(void);
static int inwhite(void);
static int inword(void);

#ifdef	OLDWAY
/* Word wrap on n-spaces.
 * Back-over whatever precedes the point on the current line and
 * stop on the first word-break or the beginning of the line.
 * If we reach the beginning of the line, jump back to the end of the
 * word and start a new line.  Otherwise, break the line at the
 * word-break, eat it, and jump back to the end of the word.
 *	NOTE:  This function may leaving trailing blanks.
 * Returns TRUE on success, FALSE on errors.
 */
int wrapword(void)
{
	register int cnt;
	struct LINE *oldp;

	oldp = curwp->w_dotp;
	cnt = -1;
	do {				
		cnt++;
		if (!backchar(FALSE, 1))
			return FALSE;
	} while (!inword());
	if (!backword(FALSE, 1))
		return FALSE;
	if (oldp == curwp->w_dotp && curwp->w_doto) {
		if (!backdel(FALSE, 1))
			return FALSE;
		if (!newline(FALSE, 1))
			return FALSE;
	}
	return forwword(FALSE, 1) && gotoeol(FALSE, 1);
}
#else
/* Word wrap on n-spaces.
 * Back-over whatever precedes the point on the current line and
 * stop on the first whitespace or the beginning of the line.
 * If we reach the beginning of the line, jump back to the end of the
 * word and start a new line.  Otherwise, break the line at the
 * whitespace, eat it, and jump back to the end of the word.
 *	NOTE:  This function may leaving trailing blanks.
 * Returns TRUE on success, FALSE on errors.
 */
int wrapword(void)
{
	register int cnt;
	struct LINE *oldp;

	oldp = curwp->w_dotp;
	cnt = -1;
	do {				
		cnt++;
		if (!backchar(FALSE, 1))
			return FALSE;
	} while (!inwhite());
	if (!backwhite())
		return FALSE;
	if (oldp == curwp->w_dotp && curwp->w_doto) {
		if (!backdel(FALSE, 1))
			return FALSE;
		if (!newline(FALSE, 1))
			return FALSE;
	}
	return forwword(FALSE, 1) && gotoeol(FALSE, 1);
}

/*
 * Return TRUE if the character at dot is a character that is considered
 * to be white space. Used in wordwrap.
 * The whitespace character list is hard coded. Should be setable.
 */
static int inwhite(void)
{
	register int	c;

	if (curwp->w_doto == llength(curwp->w_dotp))
		return FALSE;
	c = lgetc(curwp->w_dotp, curwp->w_doto);
	if (c == ' ' || c == '\t' )
		return TRUE;
	return FALSE;
}

/*
 * Move the cursor backward to the last bit of whitespace...
 * All of the details of motion are performed by the "backchar" and "forwchar"
 * routines. Error if you try to move beyond the buffers.
 */
static int backwhite(void)
{
	if (backchar(FALSE, 1) == FALSE)
		return FALSE;
	while (!inwhite()) {
		if (backchar(FALSE, 1) == FALSE)
			return FALSE;
	}
	return forwchar(FALSE, 1);
}
#endif

/*
 * Move the cursor backward by "n" words.  All of the details of motion
 * are performed by the "backchar" and "forwchar" routines.  Error if you
 * try to move beyond the buffers.
 */
int backword(int f, int n)
{
	if (n < 0)
		return forwword(f, -n);
	if (backchar(FALSE, 1) == FALSE)
		return FALSE;
	while (n--) {
		while (inword() == FALSE) {
			if (backchar(FALSE, 1) == FALSE)
				return FALSE;
		}
		while (inword() != FALSE) {
			if (backchar(FALSE, 1) == FALSE)
				return FALSE;
		}
	}
	return forwchar(FALSE, 1);
}

/*
 * Move the cursor forward by the specified number of words.  All of the
 * motion is done by "forwchar".  Error if you try and move beyond the
 * buffer's end.
 */
int forwword(int f, int n)
{
	if (n < 0)
		return backword(f, -n);
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FALSE, 1) == FALSE)
				return FALSE;
		}
		while (inword() != FALSE) {
			if (forwchar(FALSE, 1) == FALSE)
				return FALSE;
		}
	}
	return TRUE;
}

/*
 * Move the cursor forward by the specified number of words.  As you move,
 * convert any characters to upper case.  Error if you try and move beyond
 * the end of the buffer.
 * Bound to "M-U".
 */
int upperword(int f, int n)
{
	register int	c;

	if (n < 0)
		return FALSE;
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FALSE, 1) == FALSE)
				return FALSE;
		}
		while (inword() != FALSE) {
			c = lgetc(curwp->w_dotp, curwp->w_doto);
			if (c >= 'a' && c <= 'z') {
				if (!lchange(WFHARD))
					return FALSE;
				c -= 'a' - 'A';
				lputc(curwp->w_dotp, curwp->w_doto, c);
			}
			if (forwchar(FALSE, 1) == FALSE)
				return FALSE;
		}
	}
	return TRUE;
}

/*
 * Move the cursor forward by the specified number of words.  As you move
 * convert characters to lower case.  Error if you try and move over the
 * end of the buffer.
 * Bound to "M-L".
 */
int lowerword(int f, int n)
{
	register int	c;

	if (n < 0)
		return FALSE;
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FALSE, 1) == FALSE)
				return FALSE;
		}
		while (inword() != FALSE) {
			c = lgetc(curwp->w_dotp, curwp->w_doto);
			if (c >= 'A' && c <= 'Z') {
				if (!lchange(WFHARD))
					return FALSE;
				c += 'a' - 'A';
				lputc(curwp->w_dotp, curwp->w_doto, c);
			}
			if (forwchar(FALSE, 1) == FALSE)
				return FALSE;
		}
	}
	return TRUE;
}

/*
 * Move the cursor forward by
 * the specified number of words. As you move
 * convert the first character of the word to upper
 * case, and subsequent characters to lower case. Error
 * if you try and move past the end of the buffer.
 * Bound to "M-C".
 */
int capword(int f, int n)
{
	register int	c;

	if (n < 0)
		return FALSE;
	while (n--) {
		while (inword() == FALSE) {
			if (forwchar(FALSE, 1) == FALSE)
				return FALSE;
		}
		if (inword() != FALSE) {
			c = lgetc(curwp->w_dotp, curwp->w_doto);
			if (c >= 'a' && c <= 'z') {
				if (!lchange(WFHARD))
					return FALSE;
				c -= 'a' - 'A';
				lputc(curwp->w_dotp, curwp->w_doto, c);
			}
			if (forwchar(FALSE, 1) == FALSE)
				return FALSE;
			while (inword() != FALSE) {
				c = lgetc(curwp->w_dotp, curwp->w_doto);
				if (c >= 'A' && c <= 'Z') {
					if (!lchange(WFHARD))
						return FALSE;
					c += 'a' - 'A';
					lputc(curwp->w_dotp, curwp->w_doto, c);
				}
				if (forwchar(FALSE, 1) == FALSE)
					return FALSE;
			}
		}
	}
	return TRUE;
}

/*
 * Kill forward by "n" words.
 * Remember the location of dot. Move forward
 * by the right number of words. Put dot back where
 * it was and issue the kill command for the
 * right number of characters. Bound to "M-D".
 */
int delfword(int f, int n)
{
	register int	size;
	register LINE	*dotp;
	register int	doto;
#ifdef UTF8
	register LINE	*lastp;
	register int	lasto;
#endif

	if (n < 0)
		return FALSE;
	dotp = curwp->w_dotp;
	doto = curwp->w_doto;
	size = 0;
	while (n--) {
		while (inword() == FALSE) {
#ifdef UTF8
			lastp = curwp->w_dotp;
			lasto = curwp->w_doto;
#endif
			if (forwchar(FALSE, 1) == FALSE)
				return FALSE;
#ifdef UTF8
			if (lastp == curwp->w_dotp)
				size += curwp->w_doto - lasto;
			else
#endif
				++size;
		}
		while (inword() != FALSE) {
#ifdef UTF8
			lastp = curwp->w_dotp;
			lasto = curwp->w_doto;
#endif
			if (forwchar(FALSE, 1) == FALSE)
				return FALSE;
#ifdef UTF8
			if (lastp == curwp->w_dotp)
				size += curwp->w_doto - lasto;
			else
#endif
				++size;
		}
	}
/*JFM*/	if ((lastflag & CFKILL) == 0)		/* Clear kill buffer if	*/
/*JFM*/		kdelete();			/* last wasn't a kill.	*/
/*JFM*/	thisflag |= CFKILL;
	curwp->w_dotp = dotp;
	curwp->w_doto = doto;
	return ldelete(size, TRUE);
}

/*
 * Kill backwards by "n" words.
 * Move backwards by the desired number of words.
 * When dot is finally moved to its resting place,
 * fire off the kill command.
 * Bound to "M-Rubout" and to "M-Backspace".
 */
int delbword(int f, int n)
{
	register int	size;

	if (n < 0)
		return FALSE;
	if (backchar(FALSE, 1) == FALSE)
		return FALSE;
	size = 0;
	while (n--) {
		while (inword() == FALSE) {
			if (backchar(FALSE, 1) == FALSE)
				return FALSE;
			++size;
		}
		while (inword() != FALSE) {
			if (backchar(FALSE, 1) == FALSE)
				return FALSE;
			++size;
		}
	}
	if (forwchar(FALSE, 1) == FALSE)
		return FALSE;
/*JFM*/	if ((lastflag & CFKILL) == 0)		/* Clear kill buffer if	*/
/*JFM*/		kdelete();			/* last wasn't a kill.	*/
/*JFM*/	thisflag |= CFKILL;
	return ldelete(size, TRUE);
}

/*
 * Return TRUE if the character at dot
 * is a character that is considered to be
 * part of a word. The word character list is hard
 * coded. Should be setable.
 */
static int inword(void)
{
	register int	c;

	if (curwp->w_doto == llength(curwp->w_dotp))
		return FALSE;
	c = lgetc(curwp->w_dotp, curwp->w_doto);
	if (c >= 'a' && c <= 'z')
		return TRUE;
	if (c >= 'A' && c <= 'Z')
		return TRUE;
	if (c >= '0' && c <= '9')
		return TRUE;
	if (c == '$' || c == '_' || c == '\\')	/* For identifiers */
		return TRUE;
	return FALSE;
}

#if	LIBHELP
/*
 * Lookup the current word.
 * Error if you try and move past the end of the buffer.
 * Bound to "M-M-".  Count is passed to lookup routine.
 */
int lookupword(int f, int n)
{
	static char wordplace[128];
	register char *cp;
	register int	c;

	while (inword() != FALSE)		/* Get to beginning of word */
		if (backchar(FALSE, 1) == FALSE)
			break;

	while (inword() == FALSE) {		/* Advance to next word	*/
		if (forwchar(FALSE, 1) == FALSE)
			return FALSE;
	}

	cp = wordplace;
	while (inword() != FALSE) {
		*cp++ = lgetc(curwp->w_dotp, curwp->w_doto);
		if (forwchar(FALSE, 1) == FALSE)
			break;
	}
	*cp = '\0';
	return do_lookup(wordplace, f, n);
}
#endif
