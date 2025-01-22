/*
 * The functions in this file negotiate with the operating system
 * for characters, and write characters in a barely buffered fashion
 * on the display.
 * All operating systems.
 */
/*fixme: test*/
//#define CHECK_INP 1	/* check if more input */
//#define BUFF_INP 1	/* do input buffering */

#include	<stdio.h>
#include	"ed.h"

#if	IBM
#define	lo(r)	((r)&0xFF)	/* low-order byte of word register */
#define	hi(r)	((r)>>8)	/* high-order byte of word register */
#endif

#if	VMS
#include	<stsdef.h>
#include	<ssdef.h>
#include	<descrip.h>
#include	<iodef.h>
#include	<ttdef.h>
#include	<tt2def.h>

#define	NIBUF	128			/* Input  buffer size		*/
#define	NOBUF	1024			/* MM says bug buffers win!	*/
#define	EFN	0			/* Event flag			*/

char	obuf[NOBUF];			/* Output buffer		*/
int	nobuf;				/* # of bytes in above		*/
char	ibuf[NIBUF];			/* Input buffer			*/
int	nibuf;				/* # of bytes in above		*/
int	ibufi;				/* Read index			*/
int	oldmode[3];			/* Old TTY mode bits		*/
int	newmode[3];			/* New TTY mode bits		*/
short	iochan;				/* TTY I/O channel		*/
#endif

#if	CPM
#include	<bdos.h>
#endif

#if	MSDOS
#include	<dos.h>
#ifdef __VD
#include "c:\vd\vd.h"
#else
#ifdef __ZTC__
#include <disp.h>
#endif
#endif
#endif

#if	GEM
#include	<osbind.h>
#endif

#if V7
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#if defined(MIX_BSD) || defined(linux) || defined(sun)
#define SYS_V 1			/* system V */
#undef CTRL
#ifndef MIX_BSD
#ifdef linux
//#include <asm/termios.h>
#include <termios.h>
#if defined(CHECK_INP) && !defined(BUFF_INP)
#include <sys/time.h>
#endif
#else
#include <sys/termios.h>
#endif
static struct termios	ostate;			/* saved tty state */
static struct termios	nstate;			/* values for editor mode */
#include <signal.h>
#else /*MIX_BSD*/
#include <sys/termio.h>
static struct termio	ostate;			/* saved tty state */
static struct termio	nstate;			/* values for editor mode */
#endif
#if defined(CHECK_INP) && defined(BUFF_INP)
#include <fcntl.h>
#define	NIBUF	128			/* Input  buffer size		*/
char ibuf[128];
char	ibuf[NIBUF];			/* Input buffer			*/
int	nibuf;				/* # of bytes in above		*/
int	ibufi;				/* Read index			*/
#endif
#else
#define SYS_V 0			/* not system V */
#include	<sgtty.h>		/* for stty/gtty functions */
static struct	sgttyb	ostate;			/* saved tty state */
static struct	sgttyb	nstate;			/* values for editor mode */
#endif
#endif

#if TELEVIDEO
extern	int typv;			/* screen type */
#endif

#ifdef linux
void sig_handl() {}
#endif
/*
 * This function is called once
 * to set up the terminal device streams.
 * On VMS, it opens the logical 'TT', then assigns a channel to it
 * and sets it raw. The mode is set to PASTHRU (JFM), to let XON / XOFF
 * active (useful with VT101 terminals).
 * On CPM it is a no-op.
 */
void ttopen(void)
{
#if	VMS
/* JFM : simplify */
static	char	*tt = "TT:";

/*	struct	dsc$descriptor	idsc;	*/
	struct	dsc$descriptor	odsc;
/*	char	oname[40];	*/
	int	iosb[2];
	int	status;
	struct sensemode {
	  short status;
	  unsigned char xmit_baud;
	  unsigned char rcv_baud;
	  unsigned char crfill;
	  unsigned char lffill;
	  unsigned char parity;
	  unsigned char unused;
	  char class;
	  char type;
	  short scr_wid;
	  unsigned long tt_char : 24, scr_len : 8;
	  unsigned long tt2_char;
	} sg;

#if 0
	odsc.dsc$a_pointer = "SYS$INPUT";
	odsc.dsc$w_length  = strlen(odsc.dsc$a_pointer);
	odsc.dsc$b_dtype   = DSC$K_DTYPE_T;
	odsc.dsc$b_class   = DSC$K_CLASS_S;
	idsc.dsc$b_dtype   = DSC$K_DTYPE_T;
	idsc.dsc$b_class   = DSC$K_CLASS_S;
	do {
		idsc.dsc$a_pointer = odsc.dsc$a_pointer;
		idsc.dsc$w_length  = odsc.dsc$w_length;
		odsc.dsc$a_pointer = &oname[0];
		odsc.dsc$w_length  = sizeof(oname);
		status = LIB$SYS_TRNLOG(&idsc, &odsc.dsc$w_length, &odsc);
		if (status!=SS$_NORMAL && status!=SS$_NOTRAN)
			exit(status);
		if (oname[0] == 0x1B) {
			odsc.dsc$a_pointer += 4;
			odsc.dsc$w_length  -= 4;
		}
	} while (status == SS$_NORMAL);
#endif

	odsc.dsc$w_length = strlen(tt);
	odsc.dsc$a_pointer = tt;
	odsc.dsc$b_class = DSC$K_CLASS_S;
	odsc.dsc$b_dtype = DSC$K_DTYPE_T;

/*	status = SYS$ASSIGN(&odsc, &iochan, 0, 0);	*/
	status = SYS$ASSIGN(&odsc, &iochan, 3, 0);
	if (status != SS$_NORMAL)
		exit(status);
	status = SYS$QIOW(EFN, iochan, IO$_SENSEMODE, iosb, 0, 0,
			  oldmode, sizeof(oldmode), 0, 0, 0, 0);
	if (status!=SS$_NORMAL || (iosb[0]&0xFFFF)!=SS$_NORMAL)
		exit(status);
	newmode[0] = oldmode[0];
/*JFM	newmode[1] = oldmode[1] | TT$M_PASSALL | TT$M_NOECHO; */
/*JFM*/	newmode[1] = oldmode[1] | TT$M_NOECHO;
/*JFM*/	newmode[2] = oldmode[2] | TT2$M_PASTHRU;
	status = SYS$QIOW(EFN, iochan, IO$_SETMODE, iosb, 0, 0,
			  newmode, sizeof(newmode), 0, 0, 0, 0);
	if (status!=SS$_NORMAL || (iosb[0]&0xFFFF)!=SS$_NORMAL)
		exit(status);
/*JFM*/	nobuf = 0;
/*JFM*/	nibuf = ibufi = 0;

/* get screen size (from GNU emacs 'sysdep.c') */
	SYS$QIOW (EFN, iochan, IO$_SENSEMODE, &sg, 0, 0,
		&sg.class, 12, 0, 0, 0, 0);
/*2.11*/
	term.t_nrow =  sg.scr_len - 1;
	term.t_ncol = sg.scr_wid;
#endif
#if	CPM
#endif
#if	MSDOS
#if	!IBM
/*
 * Redefine cursor keys (as described in DOS Technical Reference Manual
 * p. 2-11, DOS BASIC Manual p. G-6) to mean what the user might expect.
 */
	static char *control[] = {
		"\033[0;72;16p",	/* up    = <ctrl-p>  */
		"\033[0;77;6p",		/* right = <ctrl-f>  */
		"\033[0;75;2p",		/* left  = <ctrl-b>  */
		"\033[0;80;14p",	/* down  = <ctrl-n>  */
		"\033[0;81;22p",	/* pg dn = <ctrl-v>  */
		"\033[0;73;27;86p",	/* pg up = <esc>V    */
		"\033[0;71;27;60p",	/* home  = <esc><    */
		"\033[0;79;27;62p",	/* end   = <esc>>    */
		"\033[0;83;127p",	/* del   = del       */
		"\033[0;3;27;46p"	/* <ctrl-@> = <esc>. */
	};
	register char *cp;
	register int i;

	for (i = 0; i < sizeof control / sizeof(char *); i++) {
		for (cp = control[i]; *cp; )
			ttputc(*cp++);
	}

#endif
#ifdef __VD
	vd_init(VGA1);			/* 640x480 */
	vd_set_font(VD_f6x12);		/* --> 40 row, 106 col */
	term.t_nrow = 40-1;
	term.t_ncol = 106;
#else
#ifdef __ZTC__
	disp_open();
/*07*/	term.t_nrow = disp_numrows - 1;
/*07*/	term.t_ncol = disp_numcols;
/*printf("t_nrow=%d\n", term.t_nrow); disp_flush(); disp_close(); exit();*/
#endif
#endif
#endif
#if	V7
	register char *tem;

#if defined(TIOCGWINSZ) && !defined(MIX_BSD)
/* from linux */
	struct winsize	W;

	if (ioctl(0, TIOCGWINSZ, &W) >= 0
	 && (int)W.ws_col != 0) {
		term.t_ncol = (int)W.ws_col;
/*2.11*/
		term.t_nrow = (int)W.ws_row - 1;
	} else {
#endif	/* defined(TIOCGWINSZ) */
/*06+*/	tem = (char *) getenv("TERMCAP");	/* get termcap */
	if (tem) {
		int	flag = 0;

		for (;;) {
			tem = strchr(tem, ':');
			if (!tem)
				break;
			if (tem[1] == 'l' && tem[2] == 'i') {
				if (!(flag & 1))	/* take the 1st def */
/*2.11*/
					term.t_nrow = atoi(tem+4) - 1;
				flag |= 1;
			}
			if (tem[1] == 'c' && tem[2] == 'o') {
				if (!(flag & 2))	/* take the 1st def */
					term.t_ncol = atoi(tem+4);
				flag |= 2;
			}
			if (flag == 3)
				break;
			tem++;
		}
/*06-*/	}
#if defined(TIOCGWINSZ) && !defined(MIX_BSD)
	}
#endif
#if 0 /*defined(TIOCGWINSZ) && !defined(MIX_BSD)*/
/* from linux */
	  else	{
		struct winsize	W;

		if (ioctl(0, TIOCGWINSZ, &W) >= 0
		 && (int) W.ws_col != 0) {
			term.t_ncol = (int) W.ws_col;
/*2.11*/
			term.t_nrow = (int) W.ws_row - 1;
		}
	}
#endif	/* defined(TIOCGWINSZ) */

#if SYS_V
#ifdef MIX_BSD
/*07+ Unix system V */
	ioctl(1, TCGETA, &ostate);		/* save old state */
	ioctl(1, TCGETA, &nstate);		/* get base of new state */
	nstate.c_iflag |= IGNBRK;	/* Ignore break condition */
	nstate.c_iflag &= ~(ICRNL|IXON); /* Disable map of CR to NL on input */
	nstate.c_oflag &= ~ONLCR;		/* no \r to \r\n on output */
	nstate.c_lflag &= ~(ECHO|ICANON|ISIG);	/* no echo for now... */
	nstate.c_cc[VMIN] = 1;		/* Input should wait for at least 1 char */
	nstate.c_cc[VTIME] = 0;		/* no matter how long that takes.  */
	ioctl(1, TCSETAW, &nstate);		/* set mode */
/*?? ou SETAF ou SETA ?? */
/*07-*/
#else /*linux && sun*/
/* we may receive a SIGWINCH when the window size changes */
	{
		struct sigaction sigact;

		sigemptyset(&sigact.sa_mask);
#ifndef linux
		sigact.sa_handler = SIG_IGN;
#else
		sigact.sa_handler = sig_handl;
#endif
		sigact.sa_flags = 0;
		sigaction(SIGWINCH, &sigact, NULL);
	}
/* from /usr/doc/tools-2.11/example/sysunix.c */
	tcgetattr(1, &ostate);
	nstate = ostate;
	nstate.c_cc[VINTR] = -1;
	nstate.c_cc[VQUIT] = -1;
	nstate.c_cc[VLNEXT] = -1;
	nstate.c_cc[VSTOP] = -1;
/*	nstate.c_lflag &= ~(ECHO | ICANON); */
	nstate.c_iflag &= ~(ISTRIP | INPCK);
	nstate.c_iflag |= IGNBRK;	/* Ignore break condition */
	nstate.c_iflag &= ~(ICRNL|IXON); /* Disable map of CR to NL on input */
	nstate.c_oflag &= ~ONLCR;		/* no \r to \r\n on output */
	nstate.c_lflag &= ~(ECHO|ICANON|ISIG);	/* no echo for now... */
	nstate.c_cc[VMIN] = 1;
	nstate.c_cc[VTIME] = 0;
	tcsetattr(1, TCSANOW, &nstate);
#endif
#else /*!SYS_V*/
	gtty(1, &ostate);			/* save old state */
	gtty(1, &nstate);			/* get base of new state */
	nstate.sg_flags |= RAW;
	nstate.sg_flags &= ~(ECHO|CRMOD);	/* no echo for now... */
	stty(1, &nstate);			/* set mode */
#endif
#endif
#if VT100
#if TELEVIDEO
	if (typv == 0)			/* JFM : if VT100 */
#endif
	{
		ttputc('\033');			/* JFM */
		ttputc('=');			/* keypad mode */
		ttputc('\033');
		ttputc('<');			/* mode VT100 */
	}

/*2.13*/
	/* if 80x24, try to get the real size (remote access) */
	if (term.t_nrow == 23
	    && term.t_ncol == 80) {

		/* set the cursor at the most bottom - rigth position */
		ttputc('\033');
		ttputc('[');
		ttputc('2');
		ttputc('0');
		ttputc('0');
		ttputc(';');
		ttputc('2');
		ttputc('0');
		ttputc('0');
		ttputc('H');

		/* get the cursor position */
		ttputc('\033');
		ttputc('[');
		ttputc('6');
		ttputc('n');
		ttflush();
		if (ttgetc() == '\033'
		 && ttgetc() == '[') {
			char c;
			int h, w;

			h = 0;
			for (;;) {
				c = ttgetc();
				if (c == ';')
					break;
				h = h * 10 + c - '0';
			}
			w = 0;
			for (;;) {
				c = ttgetc();
				if (c == 'R')
					break;
				w = w * 10 + c - '0';
			}
/*			printf("\r\nh:%d w:%d\n\r\n", h, w);*/
			term.t_nrow = h - 1;
			term.t_ncol = w;
		}
	}
/*printf("t_nrow=%d t_ncol:%d\n\r", term.t_nrow, term.t_ncol); ttgetc();*/
#endif
#if 0 /*fixme-test*/
	ttclose();
	exit(1);
#endif
}

/*
 * This function gets called just
 * before we go back home to the command interpreter.
 * On VMS it puts the terminal back in a reasonable state.
 * Another no-operation on CPM.
 */
void ttclose(void)
{
#if	VMS
	int	status;
	int	iosb[1];

#if VT100
#if TELEVIDEO
	if (typv == 0)			/* JFM : if VT100 */
#endif
	{
	ttputc('\033');			/* JFM */
	ttputc('>');			/* keypad mode off */
	}
#endif
	ttflush();
	status = SYS$QIOW(EFN, iochan, IO$_SETMODE, iosb, 0, 0,
	         oldmode, sizeof(oldmode), 0, 0, 0, 0);
	if (status!=SS$_NORMAL || (iosb[0]&0xFFFF)!=SS$_NORMAL)
		exit(status);
	status = SYS$DASSGN(iochan);
	if (status != SS$_NORMAL)
		exit(status);
#endif
#if	CPM
#endif
#if	MSDOS
#if	!IBM
/* Redefine cursor keys to default values. */
	static char *control[] = {
		"\033[0;72;0;72p",
		"\033[0;77;0;77p",
		"\033[0;75;0;75p",
		"\033[0;80;0;80p",
		"\033[0;81;0;81p",
		"\033[0;73;0;73p",
		"\033[0;71;0;71p",
		"\033[0;79;0;79p",
		"\033[0;83;0;83p",
		"\033[0;3;0;3p"
	};
	register char *cp;
	register int i;

	for (i = 0; i < sizeof(control)/sizeof(char *); i++) {
		for (cp = control[i]; *cp; )
			ttputc(*cp++);
	}
#endif
#ifdef __VD
	vd_term();
#else
#ifdef __ZTC__
	disp_setattr(0x07);		/* reset white on black */
	disp_eeol();			/* must do something */
	disp_flush();
	disp_close();
#endif
#endif
#endif
#if	V7
#if VT100
#if TELEVIDEO
	if (typv == 0)			/* JFM : if VT100 */
#endif
	{
	ttputc('\033');
	ttputc('>');			/* keypad mode off */
	}
#endif
#if SYS_V
#ifdef MIX_BSD
/*07 Unix system V */
	ioctl(1, TCSETAW, &ostate);
#else /*linux && sun*/
/*	tcsetattr(1, TCSANOW, &ostate);*/
	tcsetattr(1, TCSADRAIN, &ostate);
#endif
#else
	stty(1, &ostate);
#endif
#endif
}

#if !IBM
/*
 * Write a character to the display.
 * On VMS, terminal output is buffered, and
 * we just put the characters in the big array,
 * after cheching for overflow. On CPM terminal I/O
 * unbuffered, so we just write the byte out.
 * Ditto on MS-DOS (use the very very raw console
 * output routine).
 */
void ttputc(int c)
{
#if COLOR
	c &= 0xff;
#endif
#if	VMS
	if (nobuf >= NOBUF)
		ttflush();
	obuf[nobuf++] = c;
#endif
#if	CPM
	bios(BCONOUT, c, 0);
#endif
#if	GEM
#if	NATIVE
	Bconout(2, c);
#else
	Crawio(c);
#endif
#endif
#if	MSDOS
#ifdef __VD
	vd_putc(c);
#else
#ifdef __ZTC__
	disp_putc(c);
#else
	dosb(CONDIO, c, 0);
#endif
#endif
#endif
#if	V7
	fputc(c, stdout);
#endif
}
#endif

/*
 * Flush terminal buffer. Does real work
 * where the terminal output is buffered up. A
 * no-operation on systems where byte at a time
 * terminal I/O is done.
 */
void ttflush(void)
{
#if	VMS
	int	status;
	int	iosb[2];

	status = SS$_NORMAL;
	if (nobuf != 0) {
		status = SYS$QIOW(EFN, iochan, IO$_WRITELBLK|IO$M_NOFORMAT,
			 iosb, 0, 0, obuf, nobuf, 0, 0, 0, 0);
		if (status == SS$_NORMAL)
			status = iosb[0] & 0xFFFF;
		nobuf = 0;
	}
/*	return (status); */
#endif
#if	CPM
#endif
#if	MSDOS
#ifdef __ZTC__
#ifndef __VD
	disp_flush();
#endif
#endif
#endif
#if	V7
	fflush(stdout);
#endif
}

/*
 * Read a character from the terminal,
 * performing no editing and doing no echo at all.
 * More complex in VMS that almost anyplace else, which
 * figures. Very simple on CPM, because the system can
 * do exactly what you want.
 */
int ttgetc(void)
{
#if	VMS
	int	status;
	int	iosb[2];
	int	term[2];

	while (ibufi >= nibuf) {
		ibufi = 0;
		term[0] = 0;
		term[1] = 0;
		status = SYS$QIOW(EFN, iochan, IO$_READLBLK|IO$M_TIMED,
			 iosb, 0, 0, ibuf, NIBUF, 0, term, 0, 0);
		if (status != SS$_NORMAL)
			exit(status);
		status = iosb[0] & 0xFFFF;
		if (status!=SS$_NORMAL && status!=SS$_TIMEOUT)
			exit(status);
		nibuf = (iosb[0]>>16) + (iosb[1]>>16);
		if (nibuf == 0) {
			status = SYS$QIOW(EFN, iochan, IO$_READLBLK,
				 iosb, 0, 0, ibuf, 1, 0, term, 0, 0);
			if (status != SS$_NORMAL
			|| (status = (iosb[0]&0xFFFF)) != SS$_NORMAL)
				exit(status);
			nibuf = (iosb[0]>>16) + (iosb[1]>>16);
		}
	}
	return (ibuf[ibufi++] & 0xff);		/* Allow multinational	*/
#endif
#if	CPM
	return (biosb(BCONIN, 0, 0));
#endif
#if	GEM
	register long c;

#if	NATIVE
	c = Bconin(2);
#else
	c = Crawcin();
#endif
	/*
	 * Convert arrow keys to ctrl-p,n,f,b, and function keys to
	 * various things.  (No longer return 0 for function keys.)
	 */
	switch ((int)(c >> 16)) {
	case 0x48:			/* Up arrow to ^P	*/
		return(0x10);
	case 0x50:			/* Down arrow to ^N	*/
		return(0x0E);
	case 0x4D:			/* Right arrow to ^F	*/
		return(0x06);
	case 0x4B:			/* Left arrow to ^B	*/
		return(0x02);
#if	EXKEYS
	case 0x3B:			/* F1			*/
		return(FN1);
	case 0x3C:			/* F2			*/
		return(FN2);
	case 0x3D:			/* F3			*/
		return(FN3);
	case 0x3E:			/* F4			*/
		return(FN4);
	case 0x3F:			/* F5			*/
		return(FN5);
	case 0x40:			/* F6			*/
		return(FN6);
	case 0x41:			/* F7			*/
		return(FN7);
	case 0x42:			/* F8			*/
		return(FN8);
	case 0x43:			/* F9			*/
		return(FN9);
	case 0x44:			/* F10			*/
		return(FN10);
	case 0x47:			/* Clr/Home		*/
		return(FN1D);		/*  Delete this window	*/
	case 0x52:			/* Insert		*/
		return(FN1C);		/*  perform macro	*/
	case 0x61:			/* Undo			*/
		return(FN1B);		/*  Remove help window	*/
	case 0x62:			/* Help			*/
		return(FN1A);		/*  Prompt for help	*/
#else
	case 0x3B:			/* F1			*/
	case 0x3C:			/* F2			*/
	case 0x3D:			/* F3			*/
	case 0x3E:			/* F4			*/
	case 0x3F:			/* F5			*/
	case 0x40:			/* F6			*/
	case 0x41:			/* F7			*/
	case 0x42:			/* F8			*/
	case 0x43:			/* F9			*/
	case 0x44:			/* F10			*/
		return(0x07);		/*  Ctrl-G		*/
	case 0x47:			/* Clr/Home		*/
		return(META|'1');	/*  Delete this window	*/
	case 0x52:			/* Insert		*/
		return(CTLX|'E');	/*  perform macro	*/
	case 0x61:			/* Undo			*/
		return(META|'2');	/*  Remove help window	*/
	case 0x62:			/* Help			*/
		return(META|'?');	/*  Prompt for help	*/
#endif
	default:			/* Return the keyboard character */
		return((int)c);
	}
#endif
#if	MSDOS
#if	IBM
	unsigned i;

    for(;;) {	/* try again on bad stuff */
	/* Read a character through IBM PC ROM BIOS keyboard interrupt. */
	i = ibmrbkey(0);			/* read character fn */
	if(lo(i))
		return(lo(i));			/* got a char */

	switch(hi(i)) {				/* else translate scan code */
		case 03:			/* <ctl-@> = <esc>. */
			return '.' | META;
		case 0x2D :			/* Alt X : quit */
			return 'Z' | CTRL;
		case 0x47 :			/* Home	= line start */
			return 'A' | CTRL;
		case 72:			/* up   = previosu line */
			return 'P' | CTRL;
		case 73:			/* PgUp =  page up */
			return 'V' | META;
		case 0x77:			/* Ctrl home, */
		case 0x84 :			/* Ctrl PgUp: start buffer */
			return '<' | META;
		case 75:			/* left */
			return 'B' | CTRL;
		case 0x73 :			/* Ctrl left: left word */
			return 'B' | META;
		case 77:			/* right */
			return 'F' | CTRL;
		case 0x74 :			/* Ctrl right: right word */
			return 'F' | META;
		case 79 :			/* End: end of line */
			return 'E' | CTRL;
		case 80:			/* down: next line */
			return 'N' | CTRL;
		case 81:			/* PgDn: forwards screen */
			return 'V' | CTRL;
		case 0x75:			/* Ctrl End, */
		case 0x76:			/* Ctrl PgDn: buffer end */
			return '>' | META;
		case 83:			/* del: del current */
			return 'D' | CTRL;
/*		case 0x3B :			 F1 */
		case 0x3C :			/* F2 : search */
			return 'S' | CTRL;
		case 0x3D :			/* F3 : mark */
			return '.' | META;
		case 0x3E :			/* F4 : copy buffer */
			return 'W' | META;
		case 0x3F :			/* F5 : kill buff */
			return 'W' | CTRL;
		case 0x40 :			/* F6 : Yank back */
			return 'Y' | CTRL;
		case 0x59 :			/* Shft F6: next window */
			return CTLX | 'N';
/*		case 0x44 :			 F10 */
	}
    }
#else
	return (dosb(CONRAW, 0, 0));
#endif
#endif
#if	V7
#if defined(CHECK_INP) && defined(BUFF_INP)
	int st;

	if (ibufi >= nibuf) {
		ibufi = 0;
		st = fcntl(0, F_GETFL);
		if (!(st & O_NONBLOCK)) {
			st |= O_NONBLOCK;
			fcntl(0, F_SETFL, st);
		}
		nibuf = read(0, ibuf, NIBUF);
		if (nibuf <= 0) {
			st &= ~O_NONBLOCK;
			fcntl(0, F_SETFL, st);
			nibuf = read(0, ibuf, 1);
		}
	}
	return ibuf[ibufi++] & 0xff;
#else
	return(fgetc(stdin));
#endif
#endif
}

#if V7
/* -- check if any input character pending -- */
int input_pending(void)
{
#if defined(CHECK_INP)
#if defined(BUFF_INP)
//fixme: dos not work with paste X selection (no screen update)
	return ibufi < nibuf;
#else
//fixme: dos not work with paste X selection (too slow)
	fd_set rmask;
	struct timeval to;

	FD_ZERO(&rmask);
	FD_SET(0, &rmask);
	to.tv_sec = 0;
	to.tv_usec = 150000;		/* wait 0.15 s */
	return select(1, &rmask, 0, 0, &to) > 0;
#endif
#else
	return 0;
#endif
}
#endif
