/*
    Copyright (C) 2019 Mark Alexander

    This file is part of MicroEMACS, a small text editor.

    MicroEMACS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "def.h"
#include <excpt.h>

#if 0
#include <windef.h>
#include <winbase.h>
#include <wincon.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#endif

#define BEL 0x07                   /* BEL character.               */

static int ttattr;                 /* creen attributes             */
static int attinv  = 0x70;         /* attributes for inverse video */
       int attnorm = 7;            /* attributes for normal video  */

extern int tttop;
extern int ttbot;
extern int tthue;

/* Variables from ttyio.c */
extern int windowrow;
extern int windowcol;
extern int ttrow;
extern int ttcol;

#if GOSLING
int tceeol = 2;                    /* Costs.                       */
int tcinsl = 11;
int tcdell = 11;
#endif

/*
 * Forward declarations.
 */
#if 0
void ttinit(), tttidy(), ttmove(), tteeol(), tteeop(), ttbeep(),
     waittick(), ttwindow(), ttnowindow(), ttcolor(), ttresize(),
     ttputc(), putline();
#endif

static HANDLE hout, hin;
static int actual_nrow;
static int actual_ncol;

static void drawborders (void);
static void get_actual_pos (int row, int col, int *actual_row,
                            int *actual_col);

/*
 * Initialize the terminal.  Get the handles for console input and output.
 * Take a peek at the video buffer to see what video attributes are being used.
 */
void
ttinit (void)
{
  CHAR_INFO buf;
  COORD size, coord;
  SMALL_RECT region;
  int new_nrow, new_ncol;

  hout = GetStdHandle (STD_OUTPUT_HANDLE);
  hin =  GetStdHandle (STD_INPUT_HANDLE);

  size.X = 1;
  size.Y = 1;
  coord.X = 0;
  coord.Y = 0;
  region.Left = windowcol;
  region.Top = nrow - 1 + windowrow;
  region.Right = windowcol;
  region.Bottom = region.Top;
  if (ReadConsoleOutput (hout, &buf, size, coord, &region) == TRUE)
    {
      attnorm = buf.Attributes;		  /* current attributes   */
      attinv  = (attnorm & 0x88)          /* blink, invert bits   */
              | ((attnorm >> 4) & 0x07)   /* foreground color     */
              | ((attnorm << 4) & 0x70);  /* background color     */
    }
  ttcolor (CTEXT);

  actual_nrow = nrow;
  actual_ncol = ncol;

  if (npages < 1 || npages > 4)
    npages = 1;

  new_nrow = nrow * npages;
  new_ncol = (ncol - npages + 1) / npages;

  if (new_nrow > NROW || new_ncol <= 0)
    {
      npages = 1;
    }
  else
    {
      nrow = new_nrow;
      ncol = new_ncol;
      drawborders ();
    }
}

static void
drawborders (void)
{
  COORD coord;
  DWORD written;
  int row, col;

  for (row = 0; row < actual_nrow; ++row)
    {
      for (col = ncol; col < actual_ncol; col += ncol + 1)
        {
          coord.X = (SHORT) (col + windowcol);
          coord.Y = (SHORT) (row + windowrow);
          FillConsoleOutputCharacterW (hout, L'|', 1, coord, &written);
          FillConsoleOutputAttribute (hout, (WORD) ttattr, 1, coord,
                                     &written);
        }
    }
}

static void
get_actual_pos (int row, int col, int *actual_row, int *actual_col)
{
  if (npages > 1)
    {
      int page = row / actual_nrow;

      *actual_row = row % actual_nrow;
      *actual_col = (page * (ncol + 1)) + col;
    }
  else
    {
      *actual_row = row;
      *actual_col = col;
    }
}

/*
 * The Win32 console needs no tidy up.
 */
void
tttidy (void)
{
}

/*
 * Move the cursor to the specified
 * origin 0 row and column position. Try to
 * optimize out extra moves; redisplay may
 * have left the cursor in the right
 * location last time!
 */
void
ttmove (int row, int col)
{
  COORD coord;
  int actual_row, actual_col;

  if (ttrow!=row || ttcol!=col)
    {
      if (row >= nrow)
        row = nrow - 1;
      if (col >= ncol)
        col = ncol - 1;
      get_actual_pos (row, col, &actual_row, &actual_col);
      coord.X = (SHORT) (actual_col + windowcol);
      coord.Y = (SHORT) (actual_row + windowrow);
      SetConsoleCursorPosition (hout, coord);
      ttrow = row;
      ttcol = col;
    }
}

/*
 * Erase to end of line.
 */
void
tteeol (void)
{
  COORD coord;
  int actual_row, actual_col;
  int count;
  DWORD nwritten;

  if (ttcol >= ncol)
    return;

  get_actual_pos (ttrow, ttcol, &actual_row, &actual_col);
  count = ncol - ttcol;
  coord.X = (SHORT) (actual_col + windowcol);
  coord.Y = (SHORT) (actual_row + windowrow);
  SetConsoleTextAttribute (hout, ttattr);
  FillConsoleOutputCharacterW (hout, L' ', (DWORD) count, coord, &nwritten);
  FillConsoleOutputAttribute (hout, (WORD) ttattr, (DWORD) count, coord,
                             &nwritten);
}

/*
 * Erase to end of page.
 */
void
tteeop (void)
{
  COORD coord;
  int actual_row, actual_col;
  int count;
  int row, col;
  DWORD nwritten;

  SetConsoleTextAttribute (hout, ttattr);
  for (row = ttrow; row < nrow; ++row)
    {
      col = row == ttrow ? ttcol : 0;
      if (col >= ncol)
        continue;
      count = ncol - col;
      get_actual_pos (row, col, &actual_row, &actual_col);
      coord.X = (SHORT) (actual_col + windowcol);
      coord.Y = (SHORT) (actual_row + windowrow);
      FillConsoleOutputCharacterW (hout, L' ', (DWORD) count, coord,
                                  &nwritten);
      FillConsoleOutputAttribute (hout, (WORD) ttattr, (DWORD) count, coord,
                                 &nwritten);
    }
  drawborders ();
}

/*
 * Make a noise.
 */

void
ttbeep (void)
{
  write (1,"\007",1);
}

/*
 * No-op.
 */
void
ttwindow (int top, int bot)
{
}

/*
 * No-op.
 */
void
ttnowindow (void)
{
}

/*
 * Set display color on the Win32 console.
 */
void
ttcolor (int color)
{
  tthue = color;
#ifdef COLOR
  if (colorflag != FALSE)
    {
      switch (color)
        {
        case CMODE:
          ttattr = BACKGROUND_GREEN;
          break;
        case CKEYWORD:
          ttattr = (attnorm & 0xF0) | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
          break;
        case CSTRING:
          ttattr = (attnorm & 0xF0) | FOREGROUND_GREEN;
          break;
        case CCOMMENT:
          ttattr = (attnorm & 0xF0) | FOREGROUND_GREEN | FOREGROUND_BLUE;
          break;
        case CPREPROC:
          ttattr = (attnorm & 0xF0) | FOREGROUND_RED | FOREGROUND_BLUE;
          break;
        default:
          ttattr = attnorm;
          break;
        }
      return;
    }
#endif
  ttattr = color == CMODE ? attinv : attnorm;
}

/*
 * Resize the screen.  Pass the flag (0 or 1) to qvinit.  The flag
 * says whether to switch to 43-line or 50-line mode on EGA or VGA.
 */
void
ttresize (void)
{
  CONSOLE_SCREEN_BUFFER_INFO info;

  if (GetConsoleScreenBufferInfo (hout, &info) == TRUE
      && info.srWindow.Right >= info.srWindow.Left
      && info.srWindow.Bottom >= info.srWindow.Top)
    {
      windowrow = info.srWindow.Top;
      windowcol = info.srWindow.Left;
      nrow = info.srWindow.Bottom - windowrow + 1;
      ncol = info.srWindow.Right - windowcol + 1;
    }
  else if (actual_nrow > 0 && actual_ncol > 0)
    {
      nrow = actual_nrow;
      ncol = actual_ncol;
    }

  if (nrow <= 3)
    nrow = 25;
  else if (nrow > NROW)
    nrow = NROW;
  if (ncol <= 10)
    ncol = 80;
  else if (ncol > NCOL)
    ncol = NCOL;

  ttinit ();
}

/*
 * Write character.
 */
int
ttputc (int c)
{
  DWORD nwritten;
  char c1 = (char)c;
  SetConsoleTextAttribute (hout, ttattr);
  WriteFile (hout, &c1, 1, &nwritten, NULL);
  return c;
}

int
ttputs_color (const wchar_t *text, const short *attrs, int count)
{
  static CHAR_INFO cells[NCOL];
  CONSOLE_SCREEN_BUFFER_INFO info;
  COORD size, origin;
  SMALL_RECT region;
  int color;
  int i;

  if (count <= 0)
    return TRUE;
  if (count > NCOL)
    return FALSE;
  if (GetConsoleScreenBufferInfo (hout, &info) == FALSE)
    return FALSE;

  color = attrs[0];
  ttcolor (color);
  for (i = 0; i < count; ++i)
    {
      if (attrs[i] != color)
        {
          color = attrs[i];
          ttcolor (color);
        }
      cells[i].Char.UnicodeChar = text[i];
      cells[i].Attributes = (WORD)ttattr;
    }

  size.X = (SHORT)count;
  size.Y = 1;
  origin.X = 0;
  origin.Y = 0;
  region.Left = info.dwCursorPosition.X;
  region.Top = info.dwCursorPosition.Y;
  region.Right = region.Left + count - 1;
  region.Bottom = region.Top;
  if (WriteConsoleOutputW (hout, cells, size, origin, &region) == FALSE)
    return FALSE;

  info.dwCursorPosition.X += (SHORT)count;
  SetConsoleCursorPosition (hout, info.dwCursorPosition);
  return TRUE;
}

/*
 * High speed screen update.  row and col are 1-based.
 */
void
putline (int row, int col, const wchar_t *buf)
{
  COORD size, coord;
  SMALL_RECT region;
  static CHAR_INFO cinfo[NCOL];
  int actual_row, actual_col;
  int i;

  /* Init cinfo */
  memset (cinfo, 0, sizeof(cinfo));

  /* Adjust row and col to zero-based values.
   */
  row--;
  col--;
  if (row < 0 || row >= nrow || col < 0 || col >= ncol)
    return;

  /* The size of the data to copy is the remaining number of characters
   * on the line.
   */
  size.X = ncol - col;
  size.Y = 1;

  /* Copy the text into a char/attribute buffer. */
  for (i = 0; i < size.X; i++)
    {
      cinfo[i].Char.UnicodeChar = *buf++;
      cinfo[i].Attributes = ttattr;
    }

  get_actual_pos (row, col, &actual_row, &actual_col);
  coord.X = 0;
  coord.Y = 0;
  region.Left = (SHORT) (windowcol + actual_col);
  region.Right = (SHORT) (region.Left + size.X - 1);
  region.Top = region.Bottom = (SHORT) (windowrow + actual_row);
  WriteConsoleOutputW (hout, cinfo, size, coord, &region);
}
