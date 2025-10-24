/*
 * Buffer Menu as per Big Emacs
 * Author: Hugh Barney, hughbarney@gmail.com, March 2013
 *
 * Diplay the buffer list in a window and operate on it
 * using the followinf keys
 *
 * N,n, CTRL+N, down-arrow:   move to next line
 * P,p, CTRL+P, up-arrow:     move to prev line
 * s  save buffer at line
 * v  toggle read only flag
 * k  kill buffer at line
 * 1  select buffer at line
 * 2  select buffer in split window below original window
 * q,Q,x,X exit buffer menu
 *
 */
#include <stdlib.h>
#include <string.h>
//#include "estruct.h"
//#include "edef.h"
#include "def.h"

#define ESC    27
#define BACKSPACE 127
#define CTRL_N 14
#define CTRL_M 13
#define CTRL_P 16

extern int listbuffers(int f, int n, int k);
extern int onlywind(int f, int n, int k);
extern int splitwind(int f, int n, int k);
extern int forwline(int f, int n, int k);
extern int backline(int f, int n, int k);
extern int nextwind(int f, int n, int k);

extern int swbuffer (BUFFER *bp);
extern void update();
extern int ttgetc();
//extern void eprintf(char *,...);
extern void eerase(void);
extern int zotbuf(BUFFER *);
extern BUFFER* get_scratch(void);
extern int getctl(void);
extern int filesave (int f, int n, int k);

BUFFER *get_buffer(int );
int buffermenu(int f, int n);
int valid_buf(BUFFER*);
int count_buffers(void);


int buffermenu(int f, int n)
{
	BUFFER *bp;
	BUFFER *org_bp = curbp;
	int c,k;
	int bufptr;
	int bufcount = 0;

	bufptr = 1;

start:
	listbuffers(f, n, KRANDOM);
	swbuffer(blistp);
	onlywind(0, 0, KRANDOM);
	bufcount = count_buffers();

	if (bufptr > bufcount)
		bufptr = bufcount;

	if (bufcount > 0)
		forwline(0, bufptr + 1, KRANDOM);
	else
		forwline(0, 2, KRANDOM);

	for (;;)
	{
		eprintf("Buffer Menu: 1,2,s,v,k,q ");
		update();
		c = ttgetc();

		/* if no buffers, only allow exit */
		if (bufcount == 0)
		{
			switch (c)
			{
			case 'q': case 'Q': case 'x': case 'X':
				break;
			default:
				ttbeep();
				continue;
			}
		}

		/*
		 * pre process escape sequence to get up/down arrows
		 * convert to CTRL+N, CTRL+P
		 */
		if (c == ESC)
		{
			k = getctl();
			if (k == '[')
			{
				k = getctl();
				switch(k)
				{
				case 'A': c = CTRL_P; break;
				case 'B': c = CTRL_N; break;
				default:
					ttbeep();
					continue;
				}
			}
			else
			{
				k = getctl();
				ttbeep();
				continue;
			}
		}  /* if ESC */

		switch (c)
		{
		case 'n':
		case 'N':
		case ' ':
		case CTRL_N:
			if (bufcount == bufptr)
			{
				ttbeep();
				break;
			}
			forwline(0, 1, KRANDOM);
			bufptr++;
			break;

		case 'p':
		case 'P':
		case CTRL_P:
		case BACKSPACE:
			if (bufptr == 1)
			{
				ttbeep();
				break;
			}
			backline(0, 1, KRANDOM);
			bufptr--;
			break;

		case '1':
		case CTRL_M:
			bp = get_buffer(bufptr);
			swbuffer(bp);
			onlywind(0, 0, KRANDOM);
			eerase();
			return TRUE;

		case '2':
			bp = get_buffer(bufptr);
			swbuffer(bp);
			onlywind(0, 0, KRANDOM);
			/* need to check or is still valid */
			if (valid_buf(org_bp) == TRUE && bufcount > 1)
			{
				splitwind(0, 0, KRANDOM);
				swbuffer(org_bp);
				nextwind(0, 0, KRANDOM);
			}
			eerase();
			return TRUE;

			/* save file */
		case 's':
		case 'S':
			bp = get_buffer(bufptr);
			if (bp != NULL)
			{
				curbp = bp;
				(void)filesave(0, 0, KRANDOM);
				curbp = blistp;
				goto start;
			}
			break;

			/* toggle read only */
		case 'v':
		case 'V':
		case '%':
			bp = get_buffer(bufptr);
			if (bp != NULL)  /* be defensive */
				bp->b_flag ^= BFRO;
			goto start;
			break;

			/* kill buffer */
		case 'd':
		case 'D':
		case 'k':
		case 'K':
			bp = get_buffer(bufptr);
			if (bp != NULL)
				zotbuf(bp);
			goto start;
			break;

			/* exit buffer menu */
		case 'q':
		case 'Q':
		case 'x':
		case 'X':
			if (bufcount == 0)
			{
				bp = get_scratch();
				swbuffer(bp);
				onlywind(0, 0, KRANDOM);
				eerase();
				return TRUE;
			}

			if (valid_buf(org_bp) == TRUE)
				swbuffer(org_bp);
			else
			{
				bp = get_scratch();
				swbuffer(bp);
			}
			onlywind(0, 0, KRANDOM);
			eerase();
			return TRUE;

			/* any other key */
		default:
			ttbeep();
			break;
		}
	}

	eerase();
	return TRUE;
}

/*
 * return a the nth buffer in the list that has been walked through
 * by calling makelist.	 Will only be correct if makelist has been recently
 * been called and the list of buffers displayed.  We walk the list in the
 * same way as makelist.
 */
BUFFER *get_buffer(int n)
{
	BUFFER *bp;
	int i = 0;

	bp = bheadp;
	while (bp != NULL)
	{
		if ((bp->b_flag & BFTEMP) != 0)
		{
			bp = bp->b_bufp;
			continue;
		}

		if (++i == n)
			return bp;

		bp = bp->b_bufp;
	}

	/* we should never get here */
	eprintf("[Fatal: could not find buffer]");
	exit(1);
	return NULL;
}


/*
 * return the count of non internal buffers. Will only be correct if
 * makelist has been recently been called and the list of buffers
 * displayed.  We walk the list in the same way as makelist.
 */
int count_buffers(void)
{
	BUFFER *bp;
	int count = 0;

	bp = bheadp;
	while (bp != NULL)
	{
		if ((bp->b_flag & BFTEMP) != 0)
		{
			bp = bp->b_bufp;
			continue;
		}
		count++;
		bp = bp->b_bufp;
	}

	return count;
}

/*
 * check that the buffer is still in use and in the 
 * list of known buffers
 */
int valid_buf(BUFFER* bp_try)
{
	BUFFER *bp;

	bp = bheadp;
	while (bp != NULL)
	{
		if (bp == bp_try)
			return TRUE;  /* we found it */
		bp = bp->b_bufp;
	}

	/* if we get here the buffer must have been killed */
	return (FALSE);
}
