include <stdio.h>
#include "ed.h"

#if TERMCAP

#define NROW	24
#define NCOL	80
#define BEL	0x07
#define ESC	0x1B

static void 	tcapmove();
static void 	tcapeeol();
static void	tcapeeop();
static void	tcapbeep();
static void	tcapopen();
static void	tcapstand();
static void	tput();
static char	*tgoto();

#define TCAPSLEN 315

static char tcapbuf[TCAPSLEN];
static char	PC,
	*CM,
	*CL,
	*CE,
	*UP,
	*CD,
	*SO,
	*SE;

TERM term = {
	NROW-1,
	NCOL,
	tcapopen,
	ttclose,
	ttgetc,
	ttputc,
	ttflush,
	tcapmove,
	tcapeeol,
	tcapeeop,
	tcapbeep,
	tcapstand
};

static void tcapopen()

{
	char *getenv();
	char *t, *p, *tgetstr();
	char tcbuf[1024];
	char *tv_stype;
	char err_str[72];

	if ((tv_stype = getenv("TERM")) == NULL)
	{
		puts("Environment variable TERM not defined!");
		exit(1);
	}

	if((tgetent(tcbuf, tv_stype)) != 1)
	{
		sprintf(err_str, "Unknown terminal type %s!", tv_stype);
		puts(err_str);
		exit(1);
	}

	p = tcapbuf;
	t = tgetstr("pc", &p);
	if(t)
		PC = *t;

	CD = tgetstr("cd", &p);
	CM = tgetstr("cm", &p);
	CE = tgetstr("ce", &p);
	UP = tgetstr("up", &p);

	SO = tgetstr("so", &p);
	SE = tgetstr("se", &p);

	if(CD == NULL || CM == NULL || CE == NULL || UP == NULL)
	{
		puts("Incomplete termcap entry\n");
		exit(1);
	}

	if (p >= &tcapbuf[TCAPSLEN])
	{
		puts("Terminal description too big!\n");
		exit(1);
	}
	ttopen();
}

static void tcapmove(row, col)
register int row, col;
{
	putpad(tgoto(CM, col, row));
}

static void tcapeeol()
{
	putpad(CE);
}

static void tcapeeop()
{
	putpad(CD);
}

static void tcapbeep()
{
	ttputc(BEL);
}

static void putpad(str)
char	*str;
{
	tputs(str, 1, ttputc);
}

#if 0
static void putnpad(str, n)
char	*str;
{
	tputs(str, n, ttputc);
}
#endif

static void tcapstand(f)	/* put terminal in standout, if possible */
{				/* used for status line standouts	*/
	if (f) {
		if (SO != NULL)
			putpad(SO);
	} else {
		if (SE != NULL)
			putpad(SE);
	}
}
#endif TERMCAP
