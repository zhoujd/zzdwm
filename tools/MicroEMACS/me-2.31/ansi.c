/*
 * $Id: ansi.c,v 2.8 1993/06/09 08:44:40 moine Exp $
 * The routines in this file
 * provide support for ANSI style terminals
 * over a serial line. The serial I/O services are
 * provided by routines in "termio.c". It compiles
 * into nothing if not an ANSI device.
 *
 */
#include	<stdio.h>
#include	"ed.h"

#if	ANSI

#define	NROW	24			/* Screen size.			*/
#define	NCOL	80			/* Edit if you want to.		*/
#define	BEL	0x07			/* BEL character.		*/
#define	ESC	0x1B			/* ESC character.		*/

static void ansimove();
static void ansieeol();
static void ansieeop();
static void ansibeep();
//static void ansiopen();
static void ansistand();
static void ansiparm(register int n);

/*
 * Standard terminal interface
 * dispatch table. Most of the fields
 * point into "termio" code.
 */
TERM	term	= {
	NROW-1,
	NCOL,
	ttopen,		// ansiopen,
	ttclose,
	ttgetc,
	ttputc,
	ttflush,
	ansimove,
	ansieeol,
	ansieeop,
	ansibeep,
	ansistand
};

static void ansimove(int row, int col)
{
	ttputc(ESC);
	ttputc('[');
	ansiparm(row+1);
	ttputc(';');
	ansiparm(col+1);
	ttputc('H');
}

static void ansieeol(void)
{
	ttputc(ESC);
	ttputc('[');
	ttputc('K');
}

static void ansieeop(void)
{
	ttputc(ESC);
	ttputc('[');
	ttputc('J');
}

static void ansibeep(void)
{
	ttputc(BEL);
	ttflush();
}

static void ansiparm(register int n)
{
	register int	q;

	q = n / 10;
	if (q != 0)
		ansiparm(q);
	ttputc((n%10) + '0');
}

static void ansistand(int f)
{
	int fg;

	ttputc(ESC);		/* <ESC> [ Ps m	-- Select graphics rendition */
	ttputc('[');		/* (SGR)				*/
	if (f == 0x70 || f == 0x78) {
		ttputc('m');
		return;
	}
	fg = (f >> 4) & 0x07;
	if (f & 0x80) {
		ttputc('0');
		ttputc('1');	/* bold */
		ttputc(';');
	}
	ttputc('3');
	ttputc('0' + fg);
	if ((f & 0x08) == 0) {
		ttputc(';');
		ttputc('4');
		ttputc('0' + (f & 0x07));
	}
	ttputc('m');
}
#endif

#if 0
static void ansiopen(void)
{
/*#if	V7*/
	register char *cp;
	char *getenv();

	if ((cp = getenv("TERM")) == NULL) {
		puts("Shell variable TERM not defined!");
		exit(1);
	}
	if (strcmp(cp, "vt100") != 0
	|| strcmp(cp, "vt102") != 0) {
		puts("Terminal type not 'vt100' nor 'vt102'!");
		exit(1);
	}
//#endif
	ttopen();
}
#endif
