/*
    Copyright (C) 2008 Mark Alexander

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

/*
 * Name:	MicroEMACS
 *              Ncurses Keyboard
 *
 * By:		Mark Alexander
 *		marka@pobox.com
 *
 * $Log: ttykbd.c,v $
 * Revision 1.1  2005-10-18 02:18:44  bloovis
 * New files to implement ncurses screen handling.
 *
 *
 */

#include "def.h"
#include <ncurses.h>

#define CTRL(x) ((x) & 0x1f)

/*
 * Names for the keys with basic keycode
 * between KFIRST and KLAST (inclusive). This is used by
 * the key name routine in "kbd.c".
 */
char *keystrings[32] = {
  NULL,         "Up",           "Down",         "Left",
  "Right",      "PgUp",         "PgDn",         "Home",
  "End",        "Insert",       "Delete",       "F1",
  "F2",         "F3",           "F4",           "F5",
  "F6",         "F7",           "F8",           "F9",
  "F10",        "S-F1",         "S-F2",         "S-F3",
  "S-F4",       "S-F5",         "S-F6",         "S-F7",
  "S-F8",       "S-F9",         "S-F10",        NULL
};

/*
 * The special keys of the PC keyboard as returned by getakey()
 * are values greater than 0x100.  The following table gives
 * the values of the most commonly used special function keys.
 * These values correspond to the keys named in
 * the keystrings table above, unmodified by ALT, or CTRL keys.
 * These keys map into the MicroEMACS internal key values K01 to K1F.
 */
int specmap[32] = {
  0,         KEY_UP,    KEY_DOWN,  KEY_LEFT,
  KEY_RIGHT, KEY_PPAGE, KEY_NPAGE, KEY_HOME,
  KEY_END,   KEY_IC,    KEY_DC,    KEY_F(1),
  KEY_F(2),  KEY_F(3),  KEY_F(4),  KEY_F(5),
  KEY_F(6),  KEY_F(7),  KEY_F(8),  KEY_F(9),
  KEY_F(10), KEY_F(11), KEY_F(12), KEY_F(13),
  KEY_F(14), KEY_F(15), KEY_F(16), KEY_F(17),
  KEY_F(18), KEY_F(19), KEY_F(20), 0
};

/*
 * Read in a key, doing the low level mapping
 * of Unicode to 32 bit internal key code. This level deals with
 * mapping the special keys into their spots in the C1
 * control area. The C0 controls go right through, and
 * get remapped by "getkey".  The keys ALT-A through
 * ALT-Z are mapped to META-A through META-Z.
 */
static int mouse_decode (void);
static int mouse_decode_x10 (void);

int
getkbd (void)
{
  register int	c;
  register int	i;

  c = ttgetc ();
  if (c < KEY_MIN)			/* normal key?		*/
     return (c);			/* just return it	*/

  /* Treat backspace as Ctrl-H. */
  if (c == KEY_BACKSPACE)
    return CTRL('H');

  /* Treat window resize as Ctrl-L, which will force a screen redraw. */
  if (c == KEY_RESIZE)
    return CTRL('L');

  /* Decode mouse events into the shared mouse_* variables. */
  if (c == KEY_MAX + 3)
    return mouse_decode ();
  if (c == KEY_MAX + 6)
    return mouse_decode_x10 ();

  /* Terminals that bypass mouse reporting while Shift is held (st,
     for one) send Shift+PageUp/PageDown keys in place of the
     Shift+wheel event.  Decode them as the wheel events they stand
     for, anchored at the cursor so the window under it scrolls.
     ttrow/ttcol are already in the page-stacked space of mouse_row. */
  if (c == KEY_MAX + 4 || c == KEY_MAX + 5)
    {
      mouse_decode_sgr (c == KEY_MAX + 4 ? 64 + 4 : 64 + 4 + 1,
			ttrow, ttcol, FALSE);
      return (KMOUSE);
    }

  /* Decode bracketed paste markers (the mode is enabled by ttopen)
   * into a paste key; "pastestart" tells the paste command whether
   * this marker opens or closes the paste.
   */
  if (c == KEY_MAX + 1)
    {
      pastestart = TRUE;
      return (KPASTE);
    }
  if (c == KEY_MAX + 2)
    {
      pastestart = FALSE;
      return (KPASTE);
    }

  for (i = 0; i < 32; i++)		/* search SPECIAL map	*/
    if (c == specmap[i])		/* found it?		*/
      return (KFIRST + i);		/* return internal code */
  return (c);				/* not found, maybe Unicode? */
}

/*
 * Read and decode one SGR mouse event into the mouse_* variables
 * shared with mouseevent in display.c, and return KMOUSE.  The
 * "\033[<" prefix was already matched by the keymap (ttykeymapinit);
 * what follows is "b;x;y" and a final "M" (press or motion) or "m"
 * (release).  The event semantics live in mouse_decode_sgr
 * (display.c); this reads the bytes and translates the 1-based
 * coordinates into the page-stacked space used by the editor.
 */
static int
mouse_decode (void)
{
  char buf[40];			/* Room for "b;x;y" + final.  */
  int c = ERR;
  int n = 0;
  int release = FALSE;

  while (n < (int) sizeof (buf) - 1 && (c = ttgetc ()) != ERR)
    {
      if (c == 'M' || c == 'm')
        {
          release = (c == 'm');
          break;
        }
      buf[n++] = (char) c;
    }
  buf[n] = '\0';
  if (c == 'M' || c == 'm')
    {
      int b, x, y;
      int row, col;

      if (sscanf (buf, "%d;%d;%d", &b, &x, &y) == 3)
        {
          mouse_page_coords (x - 1, y - 1, &row, &col);
          mouse_decode_sgr (b, row, col, release);
        }
      else
        {			/* Garbled parameters; drop. */
          mouse_kind = MOUSE_MOTION;
          mouse_mods = 0;
          mouse_clicks = 0;
        }
    }
  else
    {				/* Truncated sequence; drop. */
      mouse_kind = MOUSE_MOTION;
      mouse_mods = 0;
      mouse_clicks = 0;
    }
  return (KMOUSE);
}

/*
 * Read and decode one legacy X10 mouse report: three raw bytes after
 * the "\033[M" prefix matched by the keymap, holding the button code,
 * the column, and the row, each with 32 added.  Terminals that honor
 * mode 1000 but not 1006 answer in this encoding (the Linux console,
 * older rxvt); the event semantics are the same mouse_decode_sgr as
 * for SGR.  X10 reports no releases, so a press is decoded as a press
 * immediately followed by its release, which completes the click;
 * wheel and motion events are one call each.
 */
static int
mouse_decode_x10 (void)
{
  int b, x, y;
  int row, col;

  b = ttgetc () - 32;
  x = ttgetc () - 32;
  y = ttgetc () - 32;
  mouse_page_coords (x - 1, y - 1, &row, &col);
  if ((b & (64 | 32)) == 0)
    {
      mouse_decode_sgr (b, row, col, FALSE);	/* Press...   */
      mouse_decode_sgr (b, row, col, TRUE);	/* ...click.  */
    }
  else
    mouse_decode_sgr (b, row, col, FALSE);	/* Wheel/motion. */
  return (KMOUSE);
}

/*
 * Terminal specific keymap initialization.
 * Attach the special keys to the appropriate built
 * in functions. Bind all of the assigned graphics in the
 * PC supplemental character set to "ins-self".
 * As is the case of all the keymap routines, errors
 * are very fatal.
 */
void
ttykeymapinit (void)
{
  keydup (KUP,    "back-line");
  keydup (KDOWN,  "forw-line");
  keydup (KLEFT,  "back-char");
  keydup (KRIGHT, "forw-char");
  keydup (KCTRL|KLEFT,  "back-word");
  keydup (KCTRL|KRIGHT, "forw-word");
  keydup (KPGUP,  "back-page");
  keydup (KPGDN,  "forw-page");
  keydup (KCTRL|KPGUP,  "up-window");
  keydup (KCTRL|KPGDN,  "down-window");
  keydup (KHOME,  "goto-bol");
  keydup (KEND,   "goto-eol");
  keydup (KCTRL|KHOME, "goto-bob");
  keydup (KCTRL|KEND,  "goto-eob");
  keydup (KINS,  "set-overstrike");
  keydup (KDEL,  "forw-del-char");
  keydup (KF1,   "help");
  keydup (KF2,   "file-save");
  keydup (KF3,   "file-visit");
  keydup (KF4,   "quit");
  keydup (KF5,   "revert-buffer");
  keydup (KF6,   "undo");
  keydup (KF7,   "redo");
  keydup (KF8,   "forw-buffer");
  keydup (KF9,   "search-again");
  keydup (KF10,  "only-window");
  keydup (KF11,  "find-cscope");
  keydup (KF12,  "next-cscope");
  keydup (KMOUSE, "mouse-event");
  /* Teach curses the bracketed paste markers as keys, so pasted
   * text arrives between two paste keys instead of being replayed
   * as keystrokes.  Bound to the command that inserts it as a unit.
   */
  define_key ("\033[200~", KEY_MAX + 1);
  define_key ("\033[201~", KEY_MAX + 2);
  keydup (KPASTE, "bracketed-paste");
  /* SGR mouse events have a fixed prefix and variable parameters;
     match the prefix here and read the rest in mouse_decode. */
  define_key ("\033[<", KEY_MAX + 3);
  /* The X10 prefix is claimed for the same reason, ahead of terminfo
     kmous entries: terminals without SGR (mode 1006) keep working in
     X10, and ncurses would otherwise swallow the prefix and the three
     data bytes and return an undecodable KEY_MOUSE. */
  define_key ("\033[M", KEY_MAX + 6);
  /* The shifted page keys some terminals send for Shift+wheel
     (kPRV/kNXT in terminfo) are claimed here, ahead of terminfo,
     and turned back into wheel events in getkbd. */
  define_key ("\033[5;2~", KEY_MAX + 4);
  define_key ("\033[6;2~", KEY_MAX + 5);
}
