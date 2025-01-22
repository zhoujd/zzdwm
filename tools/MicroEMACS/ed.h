/*
 * This file is the general header file for all parts of the MicroEMACS
 * display editor.  It contains definitions used by everyone, and it
 * contains the stuff you have to edit to create a version of the editor for
 * a specific operating system and terminal.
 */

#if defined(unix)
#define COHERENT
#endif
#ifdef	COHERENT
#define	V7	1			/* V7 UN*X or Coherent		*/
#define	VMS	0			/* VAX/VMS			*/
#define	CPM	0			/* CP/M-86			*/
#define	MSDOS	0			/* MS-DOS			*/
#define	GEM	0			/* GEMDOS			*/
#endif

#ifdef	GEMDOS
#define	V7	0			/* V7 UN*X or Coherent		*/
#define	VMS	0			/* VAX/VMS			*/
#define	CPM	0			/* CP/M-86			*/
#define	GEM	1			/* GEMDOS			*/
#define	MSDOS	0			/* MS-DOS			*/
#define	UPPERNM	0			/* if 0 names in all lower case	*/
#endif

#ifdef __TURBOC__
#define MSDOS 1				/* JFM */
#define IBM 1				/* JFM : not ANSI */
#define __STDC__ 1
#endif
#ifdef __ZTC__
#define MSDOS 1				/* JFM */
#define IBM 1				/* JFM : not ANSI */
#define __STDC__ 1
#endif
#ifdef	MSDOS
#if	MSDOS
#define	V7	0			/* V7 UN*X or Coherent		*/
#define	VMS	0			/* VAX/VMS			*/
#define	CPM	0			/* CP/M-86			*/
#define	GEM	0			/* GEMDOS			*/
#endif
#endif

#ifdef	COHERENT
#define ANSI	1
#define VT52	0			/* VT52 terminal (Zenith).	*/
#define	VT100	1			/* Handle VT100 style keypad.	*/
#define TELEVIDEO 0			/* Handle TV 950 */
#define TERMCAP 0			/* Use TERMCAP			*/
#define	NATIVE	0			/* Native ATARI ST screen	*/
#define	EXKEYS	0			/* No extended keys		*/
#endif

#ifdef	GEMDOS
#define ANSI	0
#define VT52	0			/* VT52 terminal (Zenith).	*/
#define	VT100	0			/* Handle VT100 style keypad.	*/
#define TELEVIDEO 0			/* Handle TV 950 */
#define TERMCAP 0			/* Use TERMCAP			*/
#define	NATIVE	1			/* Native ATARI ST screen	*/
#define	EXKEYS	1			/* Extended keys		*/
#endif

#ifdef	MSDOS
#if	MSDOS
#ifndef	IBM
#define	ANSI	1			/* Use ANSI.SYS			*/
#else
#define ANSI	0			/* don't use ANSI.SYS		*/
#endif
#define VT52	0			/* VT52 terminal (Zenith).	*/
#define	VT100	0			/* Handle VT100 style keypad.	*/
#define TELEVIDEO 0			/* Handle TV 950 */
#define TERMCAP 0			/* Use TERMCAP			*/
#define	NATIVE	0			/* Native ATARI ST screen	*/
#define	EXKEYS	0			/* No extended keys		*/
#endif
#endif

#ifdef vax
#define	V7	0			/* V7 UN*X or Coherent		*/
#define	VMS	1			/* VAX/VMS			*/
#define	CPM	0			/* CP/M-86			*/
#define	GEM	0			/* GEMDOS			*/
#define	MSDOS	0			/* MS-DOS			*/
#define	IBM	0
#define ANSI	1
#define VT52	0			/* VT52 terminal (Zenith).	*/
#define	VT100	1			/* Handle VT100 style keypad.	*/
#define TELEVIDEO 1			/* Handle TV 950 */
#define TERMCAP 0			/* Use TERMCAP			*/
#define	NATIVE	0			/* Native ATARI ST screen	*/
#define	EXKEYS	0			/* No extended keys		*/
#endif

#define	CVMVAS	1			/* C-V, M-V arg. in screens.	*/
#define	LIBHELP	0			/* Use help stuff		*/
/*JFM-2.12*/
#define OLD_NUMBER 0			/* Use old line numbering */

#define	NCFILES	5			/* Max # of files on command line */
/*jfm-2.28*/
#define	NFILEN	256			/* # of bytes, file name	*/
#define	NBUFN	16			/* # of bytes, buffer name	*/
#define	NLINE	1024			/* # of bytes, line		*/
#define	NKBDM	256			/* # of strokes, keyboard macro	*/
#define	NPAT	80			/* # of bytes, pattern		*/
#define	HUGE	1000			/* Huge number			*/
#define	NSRCH	128			/* undoable search command len	*/
#define	ERRLINES 3			/* error window lines displayed	*/

#define	METACH	0x1B			/* M- prefix,   Control-[, ESC	*/
#define	CTMECH	0x1C			/* C-M- prefix, Control-\	*/
#define	EXITCH	0x1D			/* Exit level,  Control-]	*/
#define	CTRLCH	0x1E			/* C- prefix,	Control-^	*/
#define	HELPCH	0x1F			/* Help key,    Control-_	*/

#define	CTRL	0x0100			/* Control flag, or'ed in	*/
#define	META	0x0200			/* Meta flag, or'ed in		*/
#define	CTLX	0x0400			/* ^X flag, or'ed in		*/

#define	FALSE	0			/* False, no, bad, etc.		*/
#define	TRUE	1			/* True, yes, good, etc.	*/
#define	ABORT	2			/* Death, ^G, abort, etc.	*/

#define	FIOSUC	0			/* File I/O, success.		*/
#define	FIOFNF	1			/* File I/O, file not found.	*/
#define	FIOEOF	2			/* File I/O, end of file.	*/
#define	FIOERR	3			/* File I/O, error.		*/
#define	FIORO	4			/* File I/O, read only.		*/

#define	CFCPCN	0x0001			/* Last command was C-P, C-N	*/
#define	CFKILL	0x0002			/* Last command was a kill	*/


#if	EXKEYS
/*
 * The following codes should be bound to the function keys in the system
 * specific keyboard handling (mostly in termio.c).
 *
 * When used in the key table in main.c, the same functions should be
 * available through other key sequences.
 */
#define	FN0	0x80			/* Function key 0	*/
#define	FN1	0x81			/* Function key 1	*/
#define	FN2	0x82			/* Function key 2	*/
#define	FN3	0x83			/* Function key 3	*/
#define	FN4	0x84			/* Function key 4	*/
#define	FN5	0x85			/* Function key 5	*/
#define	FN6	0x86			/* Function key 6	*/
#define	FN7	0x87			/* Function key 7	*/
#define	FN8	0x88			/* Function key 8	*/
#define	FN9	0x89			/* Function key 9	*/
#define	FNA	0x8A			/* Function key 10	*/
#define	FNB	0x8B			/* Function key 11	*/
#define	FNC	0x8C			/* Function key 12	*/
#define	FND	0x8D			/* Function key 13	*/
#define	FNE	0x8E			/* Function key 14	*/
#define	FNF	0x8F			/* Function key 15	*/
#define	FN10	0x90			/* Function key 16	*/
#define	FN11	0x91			/* Function key 17	*/
#define	FN12	0x92			/* Function key 18	*/
#define	FN13	0x93			/* Function key 19	*/
#define	FN14	0x94			/* Function key 20	*/
#define	FN15	0x95			/* Function key 21	*/
#define	FN16	0x96			/* Function key 22	*/
#define	FN17	0x97			/* Function key 23	*/
#define	FN18	0x98			/* Function key 24	*/
#define	FN19	0x99			/* Function key 25	*/
#define	FN1A	0x9A			/* Function key 26	*/
#define	FN1B	0x9B			/* Function key 27	*/
#define	FN1C	0x9C			/* Function key 28	*/
#define	FN1D	0x9D			/* Function key 29	*/
#define	FN1E	0x9E			/* Function key 30	*/
#define	FN1F	0x9F			/* Function key 31	*/
#endif

/*
 * There is a window structure allocated for every active display window.
 * The windows are kept in a big list, in top to bottom screen order, with
 * the listhead at "wheadp".  Each window contains its own values of dot and
 * mark.  The flag field contains some bits that are set by commands to guide
 * redisplay; although this is a bit of a compromise in terms of decoupling,
 * the full blown redisplay is just too expensive to run for every input
 * character.
 */
typedef	struct	WINDOW {
	struct	WINDOW *w_wndp;		/* Next window			*/
	struct	BUFFER *w_bufp;		/* Buffer displayed in window	*/
	struct	LINE *w_linep;		/* Top line in the window	*/
	struct	LINE *w_dotp;		/* Line containing "."		*/
	struct	LINE *w_markp;		/* Line containing "mark"	*/
	short	w_doto;			/* Byte offset for "."		*/
	short	w_marko;		/* Byte offset for "mark"	*/
	unsigned char w_toprow;		/* Origin 0 top row of window	*/
	unsigned char w_ntrows;		/* # of rows of text in window	*/
	char	w_force;		/* If NZ, forcing row.		*/
	char	w_flag;			/* Flags.			*/
	short	w_shift;		/* JFM : shift display */
	char	w_dum[2];		/* JFM : free bytes */
}	WINDOW;

#define	WFFORCE	0x01			/* Window needs forced reframe	*/
#define	WFMOVE	0x02			/* Movement from line to line	*/
#define	WFEDIT	0x04			/* Editing within a line	*/
#define	WFHARD	0x08			/* Better to a full display	*/
#define	WFMODE	0x10			/* Update mode line.		*/

/*
 * Text is kept in buffers. A buffer header, described
 * below, exists for every buffer in the system. The buffers are
 * kept in a big list, so that commands that search for a buffer by
 * name can find the buffer header. There is a safe store for the
 * dot and mark in the header, but this is only valid if the buffer
 * is not being displayed (that is, if "b_nwnd" is 0). The text for
 * the buffer is kept in a circularly linked list of lines, with
 * a pointer to the header line in "b_linep".
 */
typedef	struct	BUFFER {
	struct	BUFFER *b_bufp;		/* Link to next BUFFER		*/
	struct	LINE *b_dotp;		/* Link to "." LINE structure	*/
	struct	LINE *b_markp;		/* Link to "mark"		*/
	short	b_doto;			/* Offset of "." in above LINE	*/
	short	b_marko;		/* Offset of "mark"		*/
	struct	LINE *b_linep;		/* Link to the header LINE	*/
	char	b_fname[NFILEN];	/* File name			*/
	char	b_bname[NBUFN];		/* Buffer name			*/
	char	b_flag;			/* Flags			*/
	char	b_nwnd;			/* Count of windows on buffer	*/
#ifdef COLOR
	unsigned char b_mode;		/* font lock mode - values in fontlock.c */
#endif
#ifdef MMAP
	int	fn;			/* file */
	unsigned char *file;		/* file content */
#endif
}	BUFFER;

#define	BFTEMP	0x01			/* Internal temporary buffer	*/
#define	BFCHG	0x02			/* Changed since last write	*/
#define	BFERROR	0x04			/* Error file buffer		*/
#define	BFNOWRT	0x08			/* Don't write this buffer	*/
#define	BFTRUNC	0x10			/* File truncated		*/
#define	BFHELP	0x20			/* Buffer is a help buffer	*/
#define	BFFLCK	0x40			/* Font lock mode		*/

/*
 * All text is kept in circularly linked
 * lists of "LINE" structures. These begin at the
 * header line (which is the blank line beyond the
 * end of the buffer). This line is pointed to by
 * the "BUFFER". Each line contains a the number of
 * bytes in the line (the "used" size), the size
 * of the text array, and the text. The end of line
 * is not stored as a byte; it's implied. Future
 * additions will include update hints, and a
 * list of marks into the line.
 */
typedef	struct	LINE {
	struct	LINE *l_fp;		/* Link to the next line	*/
	struct	LINE *l_bp;		/* Link to the previous line	*/
	int	l_size;			/* Allocated size		*/
	int	l_used;			/* Used size			*/
/*JFM-2.12*/
#if OLD_NUMBER
/*	long	l_lnumber;		 Line number in original file	*/
/*JFM*/	short	l_lnumber;		/* Line number in original file	*/
#endif
#ifdef COLOR
	char	anchor;			/* anchor at start of line */
#endif
#ifdef MMAP
	unsigned char *l_text;		/* line content */
#else
	unsigned char l_text[2];	/* A bunch of characters.	*/
#endif
}	LINE;

#define	lforw(lp)	((lp)->l_fp)
#define	lback(lp)	((lp)->l_bp)
#define	lgetc(lp, n)	((lp)->l_text[(n)])
#define	lputc(lp, n, c)	((lp)->l_text[(n)] = (c))
#define	llength(lp)	((lp)->l_used)
#define	l_number(lp)	((lp)->l_lnumber)

/* color and UTF-8 characters */
#ifdef COLOR
#ifdef UTF8
/* characters on 4 bytes */
#define CHAR_MASK 0x00ffffff
#define ATTR_MASK 0xff000000
#define ATTR_SHIFT 24
#else
/* characters on 2 bytes */
#define CHAR_MASK 0x00ff
#define ATTR_MASK 0xff00
#define ATTR_SHIFT 8
#endif
#else
#define CHAR_MASK 0xff
#endif

/*
 * The editor communicates with the display
 * using a high level interface. A "TERM" structure
 * holds useful variables, and indirect pointers to
 * routines that do useful operations. The low level get
 * and put routines are here too. This lets a terminal,
 * in addition to having non standard commands, have
 * funny get and put character code too. The calls
 * might get changed to "termp->t_field" style in
 * the future, to make it possible to run more than
 * one terminal type.
 */
typedef	struct	{
	short	t_nrow;			/* Number of rows.		*/
	short	t_ncol;			/* Number of columns.		*/
	void	(*t_open)(void);	/* Open terminal at the start.	*/
	void	(*t_close)(void);	/* Close terminal at end.	*/
	int	(*t_getchar)(void);	/* Get character from keyboard.	*/
	void	(*t_putchar)(int c);	/* Put character to display.	*/
	void	(*t_flush)(void);	/* Flush output buffers.	*/
	void	(*t_move)(int row, int col); /* Move the cursor, origin 0. */
	void	(*t_eeol)(void);	/* Erase to end of line.	*/
	void	(*t_eeop)(void);	/* Erase to end of page.	*/
	void	(*t_beep)(void);	/* Beep.			*/
	void	(*t_stand)(int f);	/* Standout mode.		*/
} TERM;

/* Shorthand for terminal routines...	*/
#define	tputc(X)	term.t_putchar(X)	/* Put a character	*/
#define	tgetc		term.t_getchar		/* Get a character	*/
#define	tbeep		term.t_beep		/* Beep the bell	*/
#define	tstand(X)	term.t_stand(X)		/* Standout mode	*/
#define	topen		term.t_open		/* Open the terminal	*/
#define	tclose		term.t_close		/* Close the terminal	*/
#define	tmove(X,Y)	term.t_move((X),(Y))	/* Move cursor		*/
#define	teeol		term.t_eeol		/* Erase to end of line	*/
#define	teeop		term.t_eeop		/* Erase to end of page	*/
#define	tflush		term.t_flush		/* Flush output buff	*/

/* in termio.c */
extern	void	ttopen(void);		/* Forward references.		*/
extern	int	ttgetc(void);
extern	void	ttputc(int c);
extern	void	ttflush(void);
extern	void	ttclose(void);

/* Command line switch flags -- set in runswitch	*/
#define	CF_ERROR	0x0001		/* Error edit switch specified	*/
#define	CF_GRABMEM	0x1000		/* Get large hunk of RAM	*/
#define	CF_VLONG	0x2000		/* Very long screen flag	*/
#define	CF_LONGSCR	0x4000		/* Long screen flag		*/

extern	unsigned int
		runswitch;		/* Switch flags			*/

extern	int	fillcol;		/* Fill column			*/
extern	int	currow;			/* Cursor row			*/
extern	int	curcol;			/* Cursor column		*/
extern	int	thisflag;		/* Flags, this command		*/
extern	int	lastflag;		/* Flags, last command		*/
extern	int	curgoal;		/* Goal for C-P, C-N		*/
extern	int	mpresf;			/* Stuff in message line	*/
extern	int	sgarbf;			/* State of screen unknown	*/
extern	WINDOW	*curwp;			/* Current window		*/
extern	BUFFER	*curbp;			/* Current buffer		*/
extern	WINDOW	*wheadp;		/* Head of list of windows	*/
extern	BUFFER	*bheadp;		/* Head of list of buffers	*/
extern	BUFFER	*blistp;		/* Buffer for C-X C-B		*/
#if	LIBHELP
extern	BUFFER	*helpbp;		/* Buffer for help		*/
#endif
extern	BUFFER	*errbp;			/* Error file buffer		*/
extern	short	kbdm[];			/* Holds kayboard macro data	*/
extern	short	*kbdmip;		/* Input pointer for above	*/
extern	short	*kbdmop;		/* Output pointer for above	*/
extern	char	pat[];			/* Search pattern		*/
extern	TERM	term;			/* Terminal information.	*/
extern	char	*ufiles[];		/* command-line specified files	*/
extern	int	ffold;			/* Fold in search flag.		*/
extern	char	errfile[];		/* error file name		*/
#if	LIBHELP
extern	char	hfname[];		/* Help file name buffer	*/
extern	char	hiname[];		/* Help index name buffer	*/
extern	char	*helpfile;		/* Help file name		*/
extern	char	*helpindex;		/* Help index file name		*/
#endif

/* A major optimization for native GEMDOS situations...	*/
#if	GEM && NATIVE
#undef	tputc
extern	long	bios();
#define	tputc(X)	bios(3, 2, (X))	/* Faster output		*/
#endif

#if	GEM
typedef	long	kill_t;
#define	kbrealloc	lrealloc
#else
typedef	int	kill_t;
#define	kbrealloc	realloc
#endif

/* global routines */
/* in basic.c */
int backchar(int f, int n);
int backline(int f, int n);
int backpage(int f, int n);
int diff(int f, int n);
int forwchar(int f, int n);
int forwline(int f, int n);
int forwpage(int f, int n);
int gotobol(int f, int n);
int gotobob(int f, int n);
int gotoeob(int f, int n);
int gotoeol(int f, int n);
int gotoline(int f, int n);
int setmark(int f, int n);
int swapmark(int f, int n);
/* in buffer.c */
int addline(BUFFER *bp, char *text);
int anycb(void);
int bclear(BUFFER *bp);
BUFFER *bfind(char *bname, int cflag, int bflag);
int killbuffer(int f, int n);
int listbuffers(int f, int n);
int makelist(void);
int showbuffer(BUFFER *bp, WINDOW *wp, int flags);
int usebuffer(int f, int n);
/* in display.c */
void mlerase(void);
void mlputs(register char *s);
int mlreply(char *prompt, char *buf, int nbuf);
void mlwrite(char *fmt, ...);
int mlyesno(char *prompt);
void movecursor(int row, int col);
int shiftleft(int f, int n);
int shiftright(int f, int n);
void update(void);
void vtinit(void);
void vtputc(register int v);
void vttidy(void);
/* in error.c */
int nexterr(int f, int n);
int preverr(int f, int n);
int readerr(void);
/* in file.c */
int filename(int f, int n);
int fileread(int f, int n);
int filesave(int f, int n);
int filevisit(int f, int n);
int filevisitero(int f, int n);
int filewrite(int f, int n);
void makename(char *bname, char *fname);
int readin(char	*fname);
int visitfile(char *fname);
int writeout(char *fn);
/* in fileio.c */
#ifdef MMAP
int ffgetfile(unsigned char **p_file, unsigned long *p_fsize);
#else
int ffgetline(char *buf, int nbuf);
#endif
int ffputline(char *buf, int nbuf);
int ffropen(char *fn);
int ffwopen(char *fn);
int ffclose(void);
/* in fontlock.c */
void set_filemode(char *fname);
int draw_line(BUFFER *bp, LINE *lp);
/* in line.c */
int kdelete(void);
int kinsert(int c);
int kremove(kill_t n);
LINE *lalloc(int used);
int lchange(int flag);
int ldelete(int n, int kflag);
void lfree(register LINE *lp);
int linsert(int n, int c);
int lnewline(void);
/* in main.c */
int ctrlg(int f, int n);
int execute(int c, int f, int n);
/* in random.c */
int backdel(int f, int n);
int deblank(int f, int n);
int forwdel(int f, int n);
int getccol(void);
int indent(int f, int n);
int killregion(int f, int n);
int killtxt(int f, int n);
int newline(int f, int n);
int openline(int f, int n);
int quote(int f, int n);
int setfillcol(int f, int n);
int setfold(int f, int n);
int showcpos(int f, int n);
int tab(int f, int n);
int twiddle(int f, int n);
int yank(int f, int n);
/* in region.c */
int copyregion(int f, int n);
int lowerregion(int f, int n);
int upperregion(int f, int n);
/* in search.c */
int backisearch(int f, int n);
int backsearch(int f, int n);
int forwisearch(int f, int n);
int forwsearch(int f, int n);
int isearch(int dir);
int queryrepl(int f, int n);
int searchagain(int f, int n);
/* in spawn.c */
int spawn(int f, int n);
int spawncli(int f, int n);
/* in termio.c */
int input_pending(void);
/* in window.c */
int enlargewind(int f, int n);
int mvdnwind(int f, int n);
int mvupwind(int f, int n);
int nextwind(int f, int n);
int onlywind(int f, int n);
int prevwind(int f, int n);
int refresh(int f, int n);
int reposition(int f, int n);
int shrinkwind(int f, int n);
int splitwind(int f, int n);
WINDOW *wpopup(void);
int zapwind(int f, int n);
/* in word.c */
int backword(int f, int n);
int capword(int f, int n);
int delbword(int f, int n);
int delfword(int f, int n);
int forwword(int f, int n);
int lowerword(int f, int n);
int upperword(int f, int n);
int wrapword(void);
