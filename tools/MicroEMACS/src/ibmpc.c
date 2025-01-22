/*
 * $Id: ibmpc.c,v 1.3 1995/07/27 07:24:35 d3moine Exp $
 * The routines referenced in this file
 * are defined in the assembly language source "ibmbios.m".
 * Because they use IBM PC ROM BIOS interrupt 0x10 to control the screen,
 * they will not work on some IBM-compatible (non-IBM) systems.
 * They make no assumptions about screen type.
 */
#include	<stdio.h>
#include	"ed.h"

#if	IBM
#define	NROW	24		/* Screen size.			*/
#define	NCOL	80		/* Edit if you want to; also in ibmbios.m. */
#define	BEL	0x07		/* BEL character.		*/

#ifdef __TURBOC__
#include <dos.h>
#include <conio.h>
#define ttputc putch
#else
#ifdef __VD
#include <dos.h>
#include "\vd\vd.h"
#define ttputc vd_putc
#else
#ifdef __ZTC__
#include <dos.h>
#include <disp.h>
#define ttputc disp_putc
#endif
#endif
#endif
#ifdef __STDC__
static	void	ibmmove(int, int);
static	void	ibmstand(int attr);	/* toggle standout mode	*/
#else
static	void	ibmmove();
static	void	ibmstand();	/* toggle standout mode	*/
#endif
static	void	ibmeeol();
static	void	ibmeeop();
static	void	ibmbeep();

/*
 * Standard terminal interface dispatch table.
 * Most of the fields point into "termio" code.
 */
TERM	term	= {
	NROW-1,
	NCOL,
	ttopen,
	ttclose,
	ttgetc,
	ttputc,
	ttflush,
	ibmmove,
	ibmeeol,
	ibmeeop,
	ibmbeep,
	ibmstand
};

/* Beep the terminal. */
static void ibmbeep()
{
	ttputc(BEL);
	ttflush();
}
#ifdef __TURBOC__
/* JFM : en Turbo C, pas besoin de ibmbios.m (assembleur) */
static void ibmmove(row, col)
int row, col;
{
	gotoxy(col+1, row+1);
}
ibmeeol()
{
	clreol();
}
ibmeeop()
{
	clrscr();		/* ? */
}
static void ibmstand(int att)
{
	textattr(att ? 0x70 : 0x07);
}				/* a voir */
int ibmrbkey(int i)
{
	AX = i;
	__int__(0x16);
	return _AX;
} /* ibmrbkey */
#endif
#ifdef __ZTC__
#ifndef __VD
static void ibmmove(int row, int col)
{
	disp_move(row, col);
}
static void ibmeeol()
{
	disp_eeol();
}
static void ibmeeop()
{
	disp_setattr(0x17);		/* white on blue */
	disp_eeop();
}
static void ibmstand(int att)
{
/*	disp_setattr(att ? DISP_REVERSEVIDEO : DISP_NORMAL); */
	disp_setattr(att ? 0x24		/* red on green */
			: 0x17);	/* white on blue */
}
#else
static void ibmmove(int row, int col)
{
	vd_gotoxy(col, row);
}
static void ibmeeol()
{
	vd_clreol();
}
static void ibmeeop()
{
	vd_clrscr();
}
static void ibmstand(int att)
{
	vd_setattr(att ? 0x24		/* red on green */
			: 0x17);	/* white on blue */
}
#endif
int ibmrbkey(int i)
{
	union REGS r;

	r.x.ax = i;
	int86(0x16, &r, &r);
	return r.x.ax;
}
#endif
