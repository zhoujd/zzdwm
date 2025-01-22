/*
 * The routines in this file
 * provide support for Televideo 950 style terminals
 * over a serial line. The serial I/O services are
 * provided by routines in "termio.c".
 */
#include	<stdio.h>
#include	"ed.h"

#if TELEVIDEO

#define	NROW	24			/* Screen size.			*/
#define	NCOL	80			/* Edit if you want to.		*/
#define	BIAS	0x20			/* Origin 0 coordinate bias.	*/
#define	ESC	0x1B			/* ESC character.		*/
#define	BEL	0x07			/* ascii bell character		*/

static	void	tlvmove();
static	void	tlveeol();
static	void	tlveeop();
static	void	tlvbeep();
static	void	tlvopen();
static	void	tlvstand();

/*
 * Dispatch table. All the
 * hard fields just point into the
 * terminal I/O code.
 * JFM : This table is moved to 'term' when screen type is Televideo (-t)
 */
TERM	termv	= {
	NROW-1,
	NCOL,
	tlvopen,
	ttclose,
	ttgetc,
	ttputc,
	ttflush,
	tlvmove,
	tlveeol,
	tlveeop,
	tlvbeep,
	tlvstand
};

static void tlvmove(row, col)
{
	ttputc(ESC);
	ttputc('=');
	ttputc(row+32);
	ttputc(col+32);
/*	printf("\033=%c%c", row+32, col+32);*/
}

static void tlveeol()
{
	ttputc(ESC);
	ttputc('T');
/*	printf("\033T");*/
}

static void tlveeop()
{
	ttputc(ESC);
	ttputc('=');
	ttputc(' ');
	ttputc(' ');
	ttputc(ESC);
	ttputc('Y');
/*	printf("\033=  \033Y");*/
}

static void tlvbeep()
{
	ttputc(BEL);
	ttflush();
}

static void tlvstand(f)		/* Set/clear standout mode...	*/
{
	/* does not exist */
}

static void tlvopen()
{
	ttopen();
}
#endif
