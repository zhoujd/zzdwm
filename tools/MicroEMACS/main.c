/*
 * This program is in public domain; written by Dave G. Conroy.
 * This file contains the main driving routine, and some keyboard processing
 * code, for the MicroEMACS screen editor.
 *
 * Modified by Jean-Francois Moine (1990-2013)
 *	http://moinejf.free.fr
 *	mailto:moinejf@free.fr
 *
 */
#include <stdio.h>
#include <string.h>
#include "ed.h"

#if	VMS
#include	<ssdef.h>
#define	GOOD	(SS$_NORMAL)
#define	BAD	(SS$_ABORT)
#endif

#ifdef __STDC__
#include <stdlib.h>
#endif

#ifndef	GOOD
#define	GOOD	0
#define	BAD	1
#endif

int	currow;				/* Working cursor row		*/
int	curcol;				/* Working cursor column	*/
int	fillcol;			/* Current fill column		*/
int	thisflag;			/* Flags, this command		*/
int	lastflag;			/* Flags, last command		*/
int	curgoal;			/* Goal column			*/
int	ffold;				/* Fold Flag			*/
BUFFER	*curbp;				/* Current buffer		*/
WINDOW	*curwp;				/* Current window		*/
BUFFER	*bheadp;			/* BUFFER listhead		*/
WINDOW	*wheadp;			/* WINDOW listhead		*/
BUFFER	*blistp;			/* Buffer list BUFFER		*/
#if	LIBHELP
BUFFER	*helpbp;			/* Help buffer			*/
#endif
BUFFER	*errbp;				/* Error file BUFFER		*/
short	kbdm[NKBDM] = {CTLX+')'};	/* Macro			*/
short	*kbdmip;			/* Input  for above		*/
short	*kbdmop;			/* Output for above		*/
char	pat[NPAT];			/* Pattern			*/
char	errfile[NFILEN];		/* Error file name		*/

#if	LIBHELP
char	*helpfile=0;			/* Help file name ptr.		*/
char	*helpindex=0;			/* Help index file name ptr.	*/
char	hfname[NFILEN];			/* Help file name place		*/
char	hiname[NFILEN];			/* Help index file name place	*/
#endif

typedef	struct	{
	short	k_code;			/* Key code			*/
	int	(*k_fp)();		/* Routine to handle it		*/
}	KEYTAB;

#if	EXKEYS
static	int	ignore(void);		/* Ignore this function		*/
#endif

#if TELEVIDEO
int	typv = 0;		/* JFM : screen type 0:VT100, 1:Televideo */
extern TERM termv;
#endif

#if	GEM
static	int	grabmem(int f, int n);	/* Grab memory			*/
#endif
#ifdef __TURBOC__
static	int	memleft(int f, int n);	/* JFM : free memory */
#endif

static int ctlxe(int f, int n);
static int ctlxlp(int f, int n);
static int ctlxrp(int f, int n);
static int quickexit(int f, int n);
static int quit(int f, int n);

/*
 * File-name list for command line...
 */
char	*cfiles[NCFILES];		/* Command line specified files	*/
int	cfilecnt;			/* File count...		*/

/*
 * Command line switch flags...
 */
unsigned int	runswitch;

/*
 * Command table.
 * This table is *roughly* in ASCII order, left to right across the characters
 * of the command.  This expains the funny location of the control-X commands.
 */
static KEYTAB	keytab[] = {
	{CTRL|'@',		setmark},
	{CTRL|'A',		gotobol},
	{CTRL|'B',		backchar},
#if 0
/*fixme*/
	{CTRL|'C',		spawncli},	/* Run CLI in subjob.	*/
#endif
	{CTRL|'D',		forwdel},
	{CTRL|'E',		gotoeol},
	{CTRL|'F',		forwchar},
	{CTRL|'G',		ctrlg},
	{CTRL|'H',		backdel},
	{CTRL|'I',		tab},
	{CTRL|'J',		indent},
	{CTRL|'K',		killtxt},
	{CTRL|'L',		refresh},
	{CTRL|'M',		newline},
	{CTRL|'N',		forwline},
	{CTRL|'O',		openline},
	{CTRL|'P',		backline},
	{CTRL|'Q',		quote},		/* Often unreachable	*/
	{CTRL|'R',		backisearch},
	{CTRL|'S',		forwisearch},
	{CTRL|'T',		twiddle},
	{CTRL|'V',		forwpage},
	{CTRL|'W',		killregion},
	{CTRL|'Y',		yank},
/*fixme*/
	{CTRL|'Z',		quickexit},	/* quick save and exit	*/
	{CTLX|CTRL|'B',		listbuffers},
	{CTLX|CTRL|'C',		quit},		/* Hard quit.		*/
/*JFM*/	{CTLX|CTRL|'D',		diff},		/* difference */
#if 1
	{CTLX|CTRL|'F',		filevisit},
#else
	{CTLX|CTRL|'F',		filename},
#endif
	{CTLX|CTRL|'L',		lowerregion},
	{CTLX|CTRL|'O',		deblank},
	{CTLX|CTRL|'N',		mvdnwind},
	{CTLX|CTRL|'P',		mvupwind},
#if 1
	{CTLX|CTRL|'R',		filevisitero},
#else
	{CTLX|CTRL|'R',		fileread},
#endif
	{CTLX|CTRL|'S',		filesave},	/* Often unreachable	*/
	{CTLX|CTRL|'U',		upperregion},
#if 1
	{CTLX|CTRL|'V',		fileread},
#else
	{CTLX|CTRL|'V',		filevisit},
#endif
	{CTLX|CTRL|'W',		filewrite},
	{CTLX|CTRL|'X',		swapmark},
	{CTLX|CTRL|'Z',		shrinkwind},
	{CTLX|'!',		spawn},		/* Run 1 command.	*/
	{CTLX|'=',		showcpos},
	{CTLX|'(',		ctlxlp},
	{CTLX|')',		ctlxrp},
#if 1
	{CTLX|'<',		shiftleft},
	{CTLX|'>',		shiftright},
#else
	{CTLX|'<',		preverr},	/* Seek previous error	*/
	{CTLX|'>',		nexterr},	/* Seek next error	*/
#endif
	{CTLX|'0',		zapwind},
	{CTLX|'1',		onlywind},
	{CTLX|'2',		splitwind},
#if	LIBHELP
	{CTLX|'?',		promptlook},
#endif
	{CTLX|'B',		usebuffer},
	{CTLX|'C',		spawncli},
#if 0
/*JFM*/	{CTLX|'D',		zapwind},
#endif
	{CTLX|'E',		ctlxe},
	{CTLX|'F',		setfillcol},
#if 0
	{CTLX|'G',		gotoline},
#endif
	{CTLX|'K',		killbuffer},
#if 0
/*JFM*/	{CTLX|'L',		gotoline},
#endif
#if 1
	{CTLX|'N',		filename},
#else
	{CTLX|'N',		nextwind},
#endif
/*08*/	{CTLX|'O',		nextwind},
	{CTLX|'P',		prevwind},
	{CTLX|'Z',		enlargewind},
#ifdef	VT52KEYS
	{META|CTRL|'B',		backword},
	{META|CTRL|'C',		capword},
	{META|CTRL|'D',		delfword},
#else
#if 0
/*JFM*/	{META|CTRL|'B',		shiftleft},
/*JFM*/	{META|CTRL|'F',		shiftright},
#endif
#endif
	{META|CTRL|'H',		delbword},
	{META|CTRL|'R',		queryrepl},
#if	LIBHELP
	{META|CTRL|']',		hlpindex},
#endif
	{META|'!',		reposition},
	{META|'.',		setmark},
#if 0
	{META|'1',		zapwind},
#endif
#if LIBHELP
	{META|'2',		zaphelp},
#endif
#if	GEM
	{META|'+',		grabmem},
#endif
	{META|'>',		gotoeob},
	{META|'<',		gotobob},
	{META|'%',		queryrepl},
	{META|'/',		searchagain},
#if	LIBHELP
	{META|'?',		lookupword},
#endif
	{META|'@',		setfold},
#ifdef	VT52KEYS
	{META|'A',		backline},	/* VT52 arrow keys	*/
	{META|'B',		forwline},
	{META|'C',		backchar},
	{META|'D',		forwchar},
#else
	{META|'B',		backword},
	{META|'C',		capword},
	{META|'D',		delfword},
#endif
	{META|'F',		forwword},
#if OLD_NUMBER
	{META|'G',		gotofline},
#else
	{META|'G',		gotoline},
#endif
	{META|'L',		lowerword},
#ifdef __TURBOC__
/*JFM*/	{META|'M',		memleft},
#endif
	{META|'Q',		quote},
	{META|'R',		backsearch},
	{META|'S',		forwsearch},
	{META|'U',		upperword},
	{META|'V',		backpage},
	{META|'W',		copyregion},
	{META|0x7f,		delbword},
	{0x7f,			backdel}
/* Extra keys on keyboard...	*/
/* Install your own bindings	*/
#if	EXKEYS
	,FN0,			ignore,
	FN1,			ignore,
	FN2,			ignore,
	FN3,			ignore,
	FN4,			ignore,
	FN5,			ignore,
	FN6,			ignore,
	FN7,			ignore,
	FN8,			ignore,
	FN9,			ignore,
	FNA,			ignore,
	FNB,			ignore,
	FNC,			ignore,
	FND,			ignore,
	FNE,			ignore,
	FNF,			ignore,
	FN10,			ignore,
	FN11,			ignore,
	FN12,			ignore,
	FN13,			ignore,
	FN14,			ignore,
	FN15,			ignore,
	FN16,			ignore,
	FN17,			ignore,
	FN18,			ignore,
	FN19,			ignore,
	FN10,			ignore,
	FN11,			ignore,
	FN12,			ignore
#endif
};

#define	NKEYTAB	(sizeof keytab / sizeof keytab[0])

static void edinit(char bname[]);
static void argproc(int argc, char **argv);
static int getctl(void);
static int getkey(void);
static int getnum(char *prompt, int n, int *lastc);

int main(int argc, char *argv[])
{
	register int c, f, n;
	char		bname[NBUFN];

#if	MSDOS
	setkeys();
#endif
#ifndef __TURBOC__
#ifndef __ZTC__
#if	IBM
	vidnit();
#endif
#endif
#endif
	runswitch = 0;				/* Initialize the switches */
	ffold = TRUE;				/* initialize the fold flg */
	argproc(argc, argv);			/* Parse the arg list	*/
#if	GEM && NATIVE
	topen();				/* Force the length setup */
	tclose();				/* and then continue.	*/
#endif
#if	GEM
	if (runswitch & CF_GRABMEM)		/* Get largest chunk of	*/
		grabmem(0, 0);			/* memory (ST only)	*/
#endif
	strcpy(bname, "main");			/* Work out the name of	*/
	if (cfilecnt > 0)			/* the default buffer.	*/
		makename(bname, cfiles[0]);	/* Make a buffer name	*/
	vtinit();				/* Displays.		*/
/*06*/	edinit(bname);				/* Buffers, windows.	*/
	if (cfilecnt > 0) {			/* If there are files	*/
		update();			/* You have to update	*/
		readin(cfiles[0]);		/* in case "[New file]"	*/
	}
	if (cfilecnt > 1) {			/* If more than one	*/
		n = (term.t_nrow - cfilecnt - 1) / cfilecnt;
		for (c = 1; c < cfilecnt ; c++) { /* For all other files... */
			splitwind(0, 0);	/* Split this window...	*/
			if ((f = curwp->w_ntrows - n) != 0)
				shrinkwind(0, f); /* Even out the windows */
			nextwind(0, 0);		/* Go on to the next one */
			visitfile(cfiles[c]);	/* Read in that file	*/
		}
	}
	if ((runswitch & CF_ERROR) != 0) {
		splitwind(0, 0);		/* Split this window	*/
		f = curwp->w_ntrows - ERRLINES;	/* Make error window small */
		if (f > 0)
			shrinkwind(0, f);
		readerr();
		nextwind(0, 0);
		mlerase();
		update();
		nexterr(0, 1);
		update();
	}

	lastflag = 0;				/* Fake last flags.	*/
#if V7
/*JFM 2.18*/
	c = 0x100;
#endif

loop:
#if V7
/*JFM 2.18*/
	/* don't update if simple insert and any character pending */
	if ((c & 0x7f00) != 0
	 || !input_pending())
#endif
	update();				/* Fix up the screen	*/

	c = getkey();				/* Get a key		*/
	if (mpresf != FALSE) {			/* If a message there	*/
		mlerase();			/* get rid of it...	*/
		update();			/* Fix screen		*/
		if (c == ' ')			/* ITS EMACS does this	*/
			goto loop;		/*  (eat a space)	*/
	}
	f = FALSE;
	n = 1;
	if (c == (CTRL | 'U')) {		/* ^U, start argument	*/
		int	ctmp;

		f = TRUE;			/* We have a count	*/
		n = getnum("Arg", 4, &ctmp);	/* get the count	*/
		c = ctmp;			/* And get the last chr	*/
	}
	if (c == (CTRL | 'X'))			/* ^X is a prefix	*/
		c = CTLX | getctl();
	if (kbdmip != NULL) {			/* Save macro strokes.	*/
		if (c != (CTLX | ')') && kbdmip > &kbdm[NKBDM - 6]) {
			ctrlg(FALSE, 0);
			goto loop;
		}
		if (f != FALSE) {
			*kbdmip++ = CTRL | 'U';
			*kbdmip++ = n;
		}
		*kbdmip++ = c;
	}
	execute(c, f, n);			/* Do it.		*/
	goto loop;
}

/*
 * Initialize all of the buffers and windows.
 * The buffer name is passed down as an argument, because the main routine may
 * have been told to read in a file by default, and we want the buffer name to
 * be right.
 */
static void edinit(char bname[])
{
	register BUFFER	*bp;
	register WINDOW	*wp;

	bp = bfind(bname, TRUE, 0);		/* First buffer		*/
	blistp = bfind("[List]", TRUE, BFTEMP);	/* Buffer list buffer	*/
#if	LIBHELP
	helpbp = bfind("[Help]", TRUE, BFTEMP|BFHELP);	/* Help buffer	*/
#endif
	wp = (WINDOW *) malloc(sizeof(WINDOW));	/* First window		*/
	if (!bp || !wp || !blistp)
		abort();
	curbp  = bp;				/* Make this current	*/
	wheadp = wp;
	curwp  = wp;
	wp->w_wndp  = NULL;			/* Initialize window	*/
	wp->w_bufp  = bp;
	bp->b_nwnd  = 1;			/* Displayed.		*/
	wp->w_linep = bp->b_linep;
	wp->w_dotp  = bp->b_linep;
	wp->w_doto  = 0;
	wp->w_markp = NULL;
	wp->w_marko = 0;
	wp->w_toprow = 0;
	wp->w_ntrows = term.t_nrow-1;		/* "-1" for mode line.	*/
	wp->w_force = 0;
	wp->w_flag  = WFMODE|WFHARD;		/* Full.		*/
	wp->w_shift = 0;			/* JFM : no shift */
}
	
/*
 * This is the general command execution routine.
 * It handles the fake binding of all the keys to "self-insert".
 * It also clears out the "thisflag" word, and arranges to move it
 * to the "lastflag", so that the next command can look at it.
 * Return the status of command.
 */
int execute(int c, int f, int n)
{
	register KEYTAB *ktp;
	register int	status;

	ktp = &keytab[0];			/* Look in key table.	*/
	while (ktp < &keytab[NKEYTAB]) {
		if (ktp->k_code == c) {
			thisflag = 0;
			status   = (*ktp->k_fp)(f, n);
			lastflag = thisflag;
			return status;
		}
		++ktp;
	}

	/*
	 * If a space was typed, fill column is defined, the argument is non-
	 * negative, and we are now past fill column, perform word wrap.
	 */
	if (c == ' ' && fillcol > 0 && n >= 0 && getccol() > fillcol)
		wrapword();

#if MSDOS
	if (c >= ' ' && c <= 0xfe) {		/* JFM : if printable */
#else
	if ((c >= 0x20 && c <= 0x7e)		/* Self inserting.	*/
	 || (c >= 0xa0 && c <= 0xfe)) {
#endif
		if (n <= 0) {			/* Fenceposts.		*/
			lastflag = 0;
			return n == 0;
		}
		thisflag = 0;			/* For the future.	*/
#ifdef UTF8
		if (c >= 0xc0) {
			int c2, c3;

			status = FALSE;		/* (make gcc happy) */
			c2 = getkey();
			if (c >= 0xe0) {	/* 3 bytes */
				c3 = getkey();
				while (--n >= 0) {
					status = linsert(1, c);
					if (status == FALSE)
						break;
					status = linsert(1, c2);
					if (status == FALSE)
						break;
					status = linsert(1, c3);
					if (status == FALSE)
						break;
				}
			} else {		/* 2 bytes */
				while (--n >= 0) {
					status = linsert(1, c);
					if (status == FALSE)
						break;
					status = linsert(1, c2);
					if (status == FALSE)
						break;
				}
			}
		} else
#endif
		status   = linsert(n, c);
		lastflag = thisflag;
		return status;
	}
	lastflag = 0;				/* Fake last flags.	*/
	return FALSE;
}

/*
 * Read in a key.  Do the standard keyboard preprocessing.
 * Convert the keys to the internal character set.
 */
static int getkey(void)
{
	register int	c;
#if	VT100
#if TELEVIDEO
loop:
#endif
	c = tgetc();
#if TELEVIDEO
	if (typv == 0) {			/* JFM : if VT100 */
#endif
	if (c == METACH) {			/* Apply M- prefix	*/
		c = tgetc();
		if (c == '[') {			/* Arrow keys.		*/
/*2.11*/
			switch (tgetc()) {
			case 'A':		/* Up */
				return CTRL|'P';
			case 'B':		/* Down */
				return CTRL|'N';
			case 'C':		/* Right */
				return CTRL|'F';
			case 'D':		/* Left */
				return CTRL|'B';
/* Linux (console and xterm) */
			case '1':			/* Home */
			case '7':
				tgetc();
			case 'H':
				return CTRL | 'A';
			case '5':			/* PgUp */
				tgetc();
				return META | 'V';
			case '4':			/* End */
			case '8':
				tgetc();
			case 'F':
				return CTRL | 'E';
			case '6':			/* PgDn */
				tgetc();
				return CTRL | 'V';
			case '2':
				if (tgetc() != '0') {
/* Sun console to be added here if needed:
 * Home: ^[[214z	PgUp: ^[[216z
 * End: ^[[220z		PgDn: ^[[222z
 */
					break;
				}
			case '3':			/* Del */
				tgetc();
				return CTRL | 'D';
			}
/* VT100 keypad (needs xterm translations) */
		} else if (c == 'O') {
			switch (tgetc()) {
			case 'A':		/* Up */
				return CTRL | 'P';
			case 'B':		/* Down */
				return CTRL | 'N';
			case 'C':		/* Right */
				return CTRL | 'F';
			case 'D':		/* Left */
				return CTRL | 'B';
			case 'w' :		/* 7 : beginning of line */
				return CTRL | 'A';
			case 'x' :		/* 8 : previous line */
				return CTRL | 'P';
			case 'y' :		/* 9 : screen backwards */
				return META | 'V';
			case 't' :		/* 4 : left */
				return CTRL | 'B';
			case 'v' :		/* 6 : right */
				return CTRL | 'F';
			case 'q' :		/* 1 : end of line */
				return CTRL | 'E';
			case 'r' :		/* 2 : next line */
				return CTRL | 'N';
			case 's' :		/* 3 : screen forwards */
				return CTRL | 'V';
			case 'n' :		/* . : del char */
				return CTRL | 'D';
			}
		}
		if (c >= 'a' && c <= 'z')	/* Force to upper	*/
			c -= 0x20;
		if (c >= 0x00 && c <= 0x1f)	/* C0 control -> C-	*/
			c = CTRL | (c + '@');
		return META | c;
	}
#if TELEVIDEO
	} else {					/* televideo */
	 if (c == 0x01) {			/* fonct */
		c = tgetc();
		tgetc();			/* eat next char */
		switch (c) {
		case '7' :
			return CTRL|'A';	/* '7' : Home */
		case '8' :
			return CTRL|'P';	/* '8' : Up */
		case '9' :
			return META|'V';	/* '9' : PgUp */
		case '4' :
			return CTRL|'B';	/* '4' : left */
		case '6' :
			return CTRL|'F';	/* '6' : right */
		case '1' :
			return CTRL|'E';	/* '1' : End */
		case '2' :
			return CTRL|'N';	/* '2' : down */
		case '3' :
			return CTRL|'V';	/* '3' : PgDn */
		case '.' :
			return CTRL|'D';	/* '.' : del char */
		default :
			goto loop;
		}
	 }
	 if (c == METACH) {			/* Apply M- prefix	*/
		c = getctl();
		return META | c;
	 }
	}
#endif
#endif
#if !VT100
	c = tgetc();
	if (c == METACH) {			/* Apply M- prefix	*/
		c = getctl();
		return META | c;
	}
#endif
	if (c == CTRLCH) {			/* Apply C- prefix	*/
		c = getctl();
		return CTRL | c;
	}
	if (c == CTMECH) {			/* Apply C-M- prefix	*/
		c = getctl();
		return CTRL | META | c;
	}
	if (c >= 0x00 && c <= 0x1f)		/* C0 control -> C-	*/
		c = CTRL | (c + '@');
	return c;
}

/*
 * Get a key.
 * Apply control modifications
 * to the read key.
 */
static int getctl(void)
{
	register int	c;

	c = tgetc();
	if (c >= 'a' && c <= 'z')		/* Force to upper	*/
		c -= 0x20;
	else if (c >= 0x00 && c <= 0x1f)	/* C0 control -> C-	*/
		c = CTRL | (c + '@');
	return c;
}

/*
 * Fancy quit command, as implemented by Norm.
 * If the current buffer has changed do a write current buffer and exit emacs,
 * otherwise simply exit.
 */
static int quickexit(int f, int n)
{
	if ((curbp->b_flag & BFCHG) != 0	/* Changed.		*/
	 && (curbp->b_flag & (BFTEMP | BFERROR | BFNOWRT)) == 0)
						/* Real and not R/O...	*/
		filesave(f, n);
	quit(f, n);				/* conditionally quit	*/
	/* NOTREACHED */
}

/*
 * Quit command.  If an argument, always quit.  Otherwise confirm
 * if a buffer has been changed and not written out.
 * Normally bound to "C-X C-C".
 */
static int quit(int f, int n)
{
	register int	s = FALSE;

	if (f != FALSE				/* Argument forces it.	*/
	 || anycb() == FALSE			/* All buffers clean.	*/
	 || (s = mlyesno("Quit")) == TRUE) {	/* User says it's OK.	*/
		vttidy();
#if	MSDOS
		resetkeys();
#endif
		if (f != FALSE || s != FALSE)
			exit(BAD);
		else
			exit(GOOD);
	}
#if	MSDOS
	setkeys();
#endif
	return s;
}

/*
 * Begin a keyboard macro.
 * Error if not at the top level in keyboard processing.
 * Set up variables and return.
 */
static int ctlxlp(int f, int n)
{
	if (kbdmip || kbdmop) {
		mlwrite("Not now");
		return FALSE;
	}
	mlwrite("[Start macro]");
	kbdmip = &kbdm[0];
	return TRUE;
}

/*
 * End keyboard macro.  Check for the same limit conditions as the
 * above routine.  Set up the variables and return to the caller.
 */
static int ctlxrp(int f, int n)
{
	if (!kbdmip) {
		mlwrite("Not now");
		return FALSE;
	}
	mlwrite("[End macro]");
	kbdmip = NULL;
	return TRUE;
}

/*
 * Execute a macro.
 * The command argument is the number of times to loop.  Quit as
 * soon as a command gets an error.
 * Return TRUE if all ok, else FALSE.
 */
static int ctlxe(int f, int n)
{
	register int c, af, an, s;

	if (kbdmip || kbdmop) {
		mlwrite("Not now");
		return FALSE;
	}
	if (n <= 0) 
		return TRUE;
	do {
		kbdmop = &kbdm[0];
		do {
			af = FALSE;
			an = 1;
			if ((c = *kbdmop++) == (CTRL | 'U')) {
				af = TRUE;
				an = *kbdmop++;
				c  = *kbdmop++;
			}
			s = TRUE;
		} while (c != (CTLX | ')')
		      && (s = execute(c, af, an)) == TRUE);
		kbdmop = NULL;
	} while (s == TRUE && --n);
	return s;
}

/*
 * Abort.
 * Beep the beeper.
 * Kill off any keyboard macro,
 * etc., that is in progress.
 * Sometimes called as a routine,
 * to do general aborting of
 * stuff.
 */
int ctrlg(int f, int n)
{
	tbeep();
	if (kbdmip) {
		kbdm[0] = CTLX | ')';
		kbdmip  = NULL;
	}
	return ABORT;
}

#if	MSDOS
void setkeys(void)			/* redefine cursor keys */
					/* so that they make    */
					/* sense to microEMACS  */
					/* as described in IBM  */
					/* DOS tech. reference  */
					/* manual		*/
{
#if	!IBM
	static char *ctlseq[] = {
		"\033[0;72;16p",	/* up = <ctrl-p>        */
		"\033[0;77;6p",		/* right = <ctrl-f>     */
		"\033[0;75;2p",		/* left = <ctrl-b>      */
		"\033[0;80;14p",	/* down = <ctrl-n>      */
		"\033[0;81;22p",	/* pg dn = <ctrl-v>     */
		"\033[0;73;27;86p",	/* pg up = <esc>V       */
		"\033[0;71;27;60p",	/* home = <esc><        */
		"\033[0;79;27;62p",	/* end = <esc>>         */
		"\033[0;83;127p",	/* del = del            */
		"\033[0;3;27;46p",	/* <ctrl-@> = <exc>.    */
		NULL
	};
	register char **ctlp;

	for (ctlp = ctlseq; NULL != *ctlp; ctlp++)
		mlwrite(*ctlp);
#endif
}

void resetkeys(void)			/* redefine cursor keys */
					/* to default values    */
{
#if  !IBM
	static char *ctlseq[] = {
		"\033[0;72;0;72p",
		"\033[0;77;0;77p",
		"\033[0;75;0;75p",
		"\033[0;80;0;80p",
		"\033[0;81;0;81p",
		"\033[0;73;0;73p",
		"\033[0;71;0;71p",
		"\033[0;79;0;79p",
		"\033[0;83;0;83p",
		"\033[0;3;0;3p",
		NULL
	};
	register char **ctlp;

	for (ctlp = ctlseq; NULL != *ctlp; ctlp++)
		mlwrite(*ctlp);
#endif
}
#endif

static void argproc(int argc, char **argv)
{
	int i;
	char *ptr;

	cfilecnt = 0;
	for (i = 1; i < argc; i++) {
		ptr = argv[i];		/* Get this argument...		*/
		if (*ptr == '-') {	/* Is this a switch?		*/
			switch (ptr[1]) {
			case 'e':
				runswitch |= CF_ERROR;
				if (ptr[2] == 0) {
					if (++i == argc) {
						runswitch &= ~CF_ERROR;
						continue;
					}
					strcpy(errfile, argv[i]);
				} else {
					strcpy(errfile, &ptr[2]);
				}
				break;
#if	LIBHELP
			case 'h':		/* Alternate help */
				if (ptr[2] == 0) {
					if (++i == argc)
						continue;
					strcpy(hfname, argv[i]);
				} else {
					strcpy(hfname, &ptr[2]);
				}
				strcpy(hiname, hfname);
#if	GEM || MSDOS
				strcat(hfname, ".hlp");
#endif
				strcat(hiname, ".idx");
				helpfile = hfname;
				helpindex = hiname;
				break;
#endif
#if	NATIVE && GEM
			case 'l':	/* long screen	*/
				runswitch |= CF_LONGSCR;
				break;
			case 't':	/* very long screen */
				runswitch |= CF_VLONG;
				break;
#endif
#if	GEM
			case 'x':	/* grab all memory */
				runswitch |= CF_GRABMEM;
				break;
#endif
#if TELEVIDEO
			case 't': {	/* JFM : Televideo */
				register char *p, *q;
				register int j;

				typv = 1;
				p = (char *) &term;	/* std */
				q = (char *) &termv;	/* tlv */
				for (j = sizeof(term); --j >= 0;)
					*p++ = *q++;
				break;
			    }
#endif
			default:
printf("Unknown option: %s\n", ptr);
				break;
			}
					/* Process this switch		*/
		} else {		/* Otherwise...			*/
			if (cfilecnt >= NCFILES)
				continue;
			cfiles[cfilecnt++] = ptr;	/* This is a file. */
#if	GEM
			fixname(cfiles[cfilecnt-1]);
#endif
		}
	}
}

#if	EXKEYS
/*
 * Do nothing.  ("Dead")
 */
static int ignore(void)
{
	return TRUE;
}
#endif

/*
 * Get a numeric argument...
 */
static int getnum(register char *prompt,
		  register int n,
		  register int *lastc)
{
	register int mflag, c;

	mflag = 0;			/* that can be discarded. */
	mlwrite("%s: %d", prompt, n);
	while ((c = getkey()) >='0' && (c <= '9' 
				     || c == (CTRL | 'U')
				     || c == '-')){
		if (c == (CTRL | 'U')) {
			n *= 4;
		/*
		 * If dash, and start of argument string, set arg.
		 * to -1.  Otherwise, insert it.
		 */
		} else if (c == '-') {
			if (mflag)
				break;
			n = 0;
			mflag = -1;

		/*
		 * If first digit entered, replace previous argument
		 * with digit and set sign.  Otherwise, append to arg.
		 */
		} else {
			if (!mflag) {
				n = 0;
				mflag = 1;
			}
			n = 10 * n + c - '0';
		}
		mlwrite("%s: %d", prompt, (mflag >= 0) ? n : (n ? -n : -1));
	}
	*lastc = c;		/* Return the terminal char.	*/

	/*
	 * Make arguments preceded by a minus sign negative and change
	 * the special argument "^U -" to an effective "^U -1".
	 */
	if (mflag == -1) {
		if (n == 0)
			n++;
		n = -n;
	}
	return n;
}

#if	GEM
/*
 * The following routine gets around a problem with GEMDOS Malloc(),
 * it forces a single, very large Malloc() so very large files can
 * be read.
 * It is available only on the Atari ST, and is bound to M-+
 * it can also be specified using the -x switch
 */
#include <osbind.h>

static int grabmem(int f, int n)
{
	extern char *lmalloc();
	register char *p = NULL;
	register long t;

	t = Malloc(-1L);			/* How big is free memory? */
	while (!p) {				/* Until we have a block */
		t -= 4096L;			/* shrink the block */
		if (t < 2048) {			/* if too small, tell user */
			mlwrite( "[Cannot allocate memory]" );
			return 1;		/* and fail	*/
		}
		p = lmalloc(t);			/* Try to get this chunk */
	}					/* loop until success or fail */
	free(p);				/* return chunk to arena */
	mlwrite( "[Allocated %D bytes]", t );	/* tell user how much */
	return 0;				/* and return success */
}
#endif

#ifdef __TURBOC__
/*
 * Display left free memory space.
 * Bound to M-M.
 */
static int memleft(int f, int n)
{
	unsigned long ml;

	ml = farcoreleft();
	mlwrite("[free memory: %D bytes]", ml);
	return 0;
}
#endif

