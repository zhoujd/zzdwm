#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <wctype.h>

#include "../display.c"

int ncol;
int tabsize;
int numberflag;

static VIDEO row;

static LINE *long_line;
static LINE *short_line;
static LINE *cjk_line;
static LINE line_header;
static BUFFER buffer;
static EWINDOW window;
static EWINDOW second_window;

EWINDOW *curwp = &window;
BUFFER *curbp = &buffer;
EWINDOW *wheadp;

int
cisctrl (wchar_t c)
{
  return c < 0x20 || c == 0x7f;
}

int
cisword (wchar_t c)
{
  return iswalnum ((wint_t) c) || c == '$' || c == '\'';
}

void
eprintf (const char *format, ...)
{
  (void) format;
}

static int mvupwind_calls;
static int mvupwind_last_n;
static int yank_calls;

int
mvupwind (int f, int n, int k)
{
  (void) f;
  (void) k;
  ++mvupwind_calls;
  mvupwind_last_n = n;
  return TRUE;
}

int
yank (int f, int n, int k)
{
  (void) f;
  (void) n;
  (void) k;
  ++yank_calls;
  return TRUE;
}

static void
assert_pos (POS *pos, LINE *line, int offset)
{
  assert (pos->p == line);
  assert (pos->o == offset);
}

static void
send_mouse (int kind, int button, int row, int column)
{
  EMOUSE event;

  memset (&event, 0, sizeof (event));
  event.kind = kind;
  event.button = button;
  event.row = row;
  event.column = column;
  assert (mouseenqueue (&event) == TRUE);
  assert (mouseevent (FALSE, 1, 0) == TRUE);
}

/*
 * Feed a complete click gesture with the given click count.
 */
static void
send_click (int button, int row, int column, int clicks)
{
  EMOUSE event;

  memset (&event, 0, sizeof (event));
  event.kind = MOUSE_CLICK;
  event.button = button;
  event.row = row;
  event.column = column;
  event.clicks = clicks;
  assert (mouseenqueue (&event) == TRUE);
  assert (mouseevent (FALSE, 1, 0) == TRUE);
}

/*
 * Feed a mouse event with modifier bits.
 */
static void
send_mouse_mods (int kind, int button, int row, int column, int modifiers)
{
  EMOUSE event;

  memset (&event, 0, sizeof (event));
  event.kind = kind;
  event.button = button;
  event.row = row;
  event.column = column;
  event.modifiers = modifiers;
  assert (mouseenqueue (&event) == TRUE);
  assert (mouseevent (FALSE, 1, 0) == TRUE);
}

static void
reset_display (void)
{
  memset (&row, 0, sizeof (row));
  ncol = 20;
  tabsize = 8;
  leftcol = 0;
  leftmargin = 0;
  vtcol = 0;
  vtattr = CTEXT;
  vttext = row.v_text;
  vtattrs = row.v_attr;
}

static void
expect_cjk_layout (int cjk_color, int following_color)
{
  assert (vtcol == 3);
  assert (row.v_text[0] == 0x7532);
  assert (row.v_text[1] == 0);
  assert (row.v_text[2] == 'X');
  assert (row.v_attr[0] == cjk_color);
  assert (row.v_attr[1] == cjk_color);
  assert (row.v_attr[2] == following_color);
}

static LINE *
make_line (const char *text)
{
  size_t length = strlen (text);
  LINE *line = calloc (1, LINEHDR_SIZE + length);

  assert (line != NULL);
  line->l_size = length;
  line->l_used = length;
  memcpy (line->l_text, text, length);
  return line;
}

static void
reset_scroll_window (void)
{
  memset (&window, 0, sizeof (window));
  memset (&buffer, 0, sizeof (buffer));
  memset (&line_header, 0, sizeof (line_header));

  buffer.b_linep = &line_header;
  line_header.l_fp = long_line;
  line_header.l_bp = short_line;
  long_line->l_fp = short_line;
  long_line->l_bp = &line_header;
  short_line->l_fp = &line_header;
  short_line->l_bp = long_line;

  window.w_bufp = &buffer;
  window.w_linep = long_line;
  window.w_ntrows = 2;
  window.w_dot.p = long_line;
  window.w_dot.o = 3;
  curwp = &window;
  wheadp = &window;
  window.w_wndp = NULL;
}

static void
reset_mouse_windows (void)
{
  reset_scroll_window ();

  window.w_wndp = &second_window;
  window.w_toprow = 0;
  window.w_ntrows = 2;
  window.w_leftcol = 0;
  window.w_hscroll = TRUE;
  window.w_flag = 0;
  second_window.w_wndp = NULL;
  second_window.w_toprow = 3;
  second_window.w_ntrows = 2;
  second_window.w_bufp = &buffer;
  second_window.w_linep = short_line;
  second_window.w_dot.p = short_line;
  second_window.w_dot.o = 0;
  second_window.w_flag = 0;
  curwp = &window;
  curbp = &buffer;
}

int
main (void)
{
  static const uchar utf8_text[] = { 0xe7, 0x94, 0xb2, 'X' };

  reset_display ();
  vtputc_color (0x7532, CSTRING);
  vtputc_color ('X', CTEXT);
  expect_cjk_layout (CSTRING, CTEXT);

  reset_display ();
  vtattr = CSTRING;
  vtputs (utf8_text, sizeof (utf8_text));
  expect_cjk_layout (CSTRING, CSTRING);

  /* The horizontal skip is in display columns: with the view
     scrolled two columns, the wide first character is skipped whole
     and the following X lands in the first cell. */
  reset_display ();
  vscreen[0] = &row;
  vtattr = CSTRING;
  vttextmove (0, 0, 2);
  vtputs (utf8_text, sizeof (utf8_text));
  assert (row.v_text[0] == 'X');
  assert (row.v_text[1] == 0);
  assert (vtcol == 3);

  long_line = make_line ("012345678901234567890123456789");
  short_line = make_line ("short line");
  reset_scroll_window ();
  assert (scrollright (FALSE, 99, 0) == TRUE);
  assert (window.w_leftcol == 4);
  assert (scrollright (TRUE, 7, 0) == TRUE);
  assert (window.w_leftcol == 11);
  assert (scrollright (TRUE, 999, 0) == TRUE);
  assert (window.w_leftcol == 11);
  assert (scrollleft (TRUE, 999, 0) == TRUE);
  assert (window.w_leftcol == 0);
  assert (scrollleft (FALSE, 99, 0) == TRUE);
  assert (window.w_leftcol == 0);
  assert (window.w_hscroll == TRUE);
  assert ((window.w_flag & WFHARD) != 0);
  assert (window.w_dot.p == long_line);
  assert (window.w_dot.o == 3);

  reset_mouse_windows ();
  {
    /* Click in the second window moves the point there and anchors
       a drag; the release at the same cell is a plain click, so
       the mark ring goes back. */
    send_mouse (MOUSE_PRESS, 0, 4, 6);
    assert (curwp == &second_window);
    assert (curbp == &buffer);
    assert_pos (&second_window.w_dot, short_line, 6);
    assert (second_window.w_hscroll == FALSE);
    assert ((second_window.w_flag & WFMOVE) != 0);
    assert (second_window.w_mark.p != NULL);
    send_mouse (MOUSE_RELEASE, 0, 4, 6);
    assert (second_window.w_mark.p == NULL);

    /* A click past the line end clamps the point to the end. */
    send_mouse (MOUSE_PRESS, 0, 1, 30);
    assert (curwp == &window);
    assert_pos (&window.w_dot, short_line, wllength (short_line));
    send_mouse (MOUSE_RELEASE, 0, 1, 30);
    assert (window.w_mark.p == NULL);

    /* A plain click does not disturb an existing mark. */
    window.w_mark.p = short_line;
    window.w_mark.o = 3;
    send_mouse (MOUSE_PRESS, 0, 0, 4);
    assert_pos (&window.w_dot, long_line, 4);
    assert_pos (&window.w_mark, long_line, 4);
    send_mouse (MOUSE_RELEASE, 0, 0, 4);
    assert_pos (&window.w_mark, short_line, 3);
    assert (mouseselect == FALSE);	/* click ends highlight */
    window.w_mark.p = NULL;

    /* Terminals that report no motion while dragging: the press
       anchors the mark, and the release position completes the
       region. */
    send_mouse (MOUSE_PRESS, 0, 0, 6);
    send_mouse (MOUSE_RELEASE, 0, 1, 5);
    assert_pos (&window.w_dot, short_line, 5);
    assert_pos (&window.w_mark, long_line, 6);

    /* Motion during the drag extends the region by moving the
       point only, and the release keeps it. */
    send_mouse (MOUSE_PRESS, 0, 0, 6);
    assert_pos (&window.w_mark, long_line, 6);
    send_mouse (MOUSE_MOTION, 0, 1, 5);
    assert_pos (&window.w_dot, short_line, 5);
    assert_pos (&window.w_mark, long_line, 6);
    send_mouse (MOUSE_RELEASE, 0, 1, 20);
    assert_pos (&window.w_dot, short_line, 10);
    assert_pos (&window.w_mark, long_line, 6);

    /* Idle motion after the release moves nothing. */
    send_mouse (MOUSE_MOTION, 0, 0, 3);
    assert_pos (&window.w_dot, short_line, 10);

    /* The highlight stays on until a non-mouse key ends it. */
    assert (mouseselect == TRUE);
    endmouseselect ();
    assert (mouseselect == FALSE);
    assert ((window.w_flag & WFHARD) != 0);	/* repaint asked */

    /* A drag stays in the window it started in: press in the
       second window and release over the first selects nothing, so
       the mark ring goes back. */
    send_mouse (MOUSE_PRESS, 0, 3, 2);
    assert_pos (&second_window.w_dot, short_line, 2);
    assert (second_window.w_mark.p != NULL);
    send_mouse (MOUSE_RELEASE, 0, 0, 2);
    assert (curwp == &second_window);
    assert_pos (&second_window.w_dot, short_line, 2);
    assert (second_window.w_mark.p == NULL);

    /* Wheel up in the second window scrolls that window up by
       mouse-wheel-step lines. */
    send_mouse (MOUSE_WHEEL, 3, 4, 0);
    assert (curwp == &second_window);
    assert (mvupwind_calls == 1);
    assert (mvupwind_last_n == mouse_wheel_step);

    /* Wheel down in the first window scrolls that window down. */
    send_mouse (MOUSE_WHEEL, 4, 0, 0);
    assert (curwp == &window);
    assert (mvupwind_calls == 2);
    assert (mvupwind_last_n == -mouse_wheel_step);

    /* A wheel report on a mode line still scrolls that window. */
    send_mouse (MOUSE_WHEEL, 3, 3 + 2, 0);
    assert (curwp == &second_window);
    assert (mvupwind_calls == 3);
    assert (mvupwind_last_n == mouse_wheel_step);

    /* A wheel report outside every window scrolls the current one. */
    send_mouse (MOUSE_WHEEL, 3, 6, 0);
    assert (curwp == &second_window);
    assert (mvupwind_calls == 4);
    assert (mvupwind_last_n == mouse_wheel_step);

    /* mouse-wheel-step sets the number of lines per tick. */
    mouse_wheel_step = 7;
    send_mouse (MOUSE_WHEEL, 4, 1, 0);
    assert (mvupwind_calls == 5);
    assert (mvupwind_last_n == -7);
    mouse_wheel_step = 3;
  }

  /* Consecutive plain clicks on one cell select the word, then the
     whole line. */
  reset_mouse_windows ();
  {
    send_mouse (MOUSE_PRESS, 0, 3, 8);
    send_mouse (MOUSE_RELEASE, 0, 3, 8);
    assert (mouseselect == FALSE);	/* first click is plain */
    assert_pos (&second_window.w_dot, short_line, 8);
    assert (second_window.w_mark.p == NULL);
    send_mouse (MOUSE_PRESS, 0, 3, 8);
    send_mouse (MOUSE_RELEASE, 0, 3, 8);
    assert_pos (&second_window.w_mark, short_line, 6);
    assert_pos (&second_window.w_dot, short_line, 10);
    assert (mouseselect == TRUE);
    send_mouse (MOUSE_PRESS, 0, 3, 8);
    send_mouse (MOUSE_RELEASE, 0, 3, 8);
    assert_pos (&second_window.w_mark, short_line, 0);
    assert_pos (&second_window.w_dot, short_line, 10);
    assert (mouseselect == TRUE);

    /* A click on another cell starts the count over. */
    send_mouse (MOUSE_PRESS, 0, 3, 2);
    send_mouse (MOUSE_RELEASE, 0, 3, 2);
    assert_pos (&second_window.w_dot, short_line, 2);
    assert_pos (&second_window.w_mark, short_line, 0);
    assert (mouseselect == FALSE);

    /* A double click on the space selects the run of blanks. */
    send_mouse (MOUSE_PRESS, 0, 3, 5);
    send_mouse (MOUSE_RELEASE, 0, 3, 5);
    send_mouse (MOUSE_PRESS, 0, 3, 5);
    send_mouse (MOUSE_RELEASE, 0, 3, 5);
    assert_pos (&second_window.w_mark, short_line, 5);
    assert_pos (&second_window.w_dot, short_line, 6);
    assert (mouseselect == TRUE);

    /* Past the end of the line there is no word to select, so the
       second click stays a plain click. */
    send_mouse (MOUSE_PRESS, 0, 3, 20);
    send_mouse (MOUSE_RELEASE, 0, 3, 20);
    send_mouse (MOUSE_PRESS, 0, 3, 20);
    send_mouse (MOUSE_RELEASE, 0, 3, 20);
    assert_pos (&second_window.w_dot, short_line, 10);	/* clamped */
    assert_pos (&second_window.w_mark, short_line, 5);
    assert (mouseselect == FALSE);

    /* A drag in between breaks the count. */
    send_mouse (MOUSE_PRESS, 0, 3, 8);
    send_mouse (MOUSE_RELEASE, 0, 3, 8);	/* count 1 */
    send_mouse (MOUSE_PRESS, 0, 3, 8);
    send_mouse (MOUSE_MOTION, 0, 3, 7);		/* drag */
    send_mouse (MOUSE_RELEASE, 0, 3, 7);
    assert_pos (&second_window.w_mark, short_line, 8);	/* kept */
    send_mouse (MOUSE_PRESS, 0, 3, 8);
    send_mouse (MOUSE_RELEASE, 0, 3, 8);	/* count 1 again */
    assert_pos (&second_window.w_mark, short_line, 8);
    assert (mouseselect == FALSE);

    /* Idle motion between clicks breaks the count too. */
    send_mouse (MOUSE_PRESS, 0, 3, 2);
    send_mouse (MOUSE_RELEASE, 0, 3, 2);	/* count 1 elsewhere */
    send_mouse (MOUSE_PRESS, 0, 3, 8);
    send_mouse (MOUSE_RELEASE, 0, 3, 8);	/* count 1 */
    send_mouse (MOUSE_MOTION, 0, 3, 0);		/* idle */
    send_mouse (MOUSE_PRESS, 0, 3, 8);
    send_mouse (MOUSE_RELEASE, 0, 3, 8);	/* still count 1 */
    assert_pos (&second_window.w_mark, short_line, 8);
    assert (mouseselect == FALSE);

    /* A release that drifts a cell from the pressed cell without
       any motion between them is a settling click, not a drag: the
       count continues, so the double click still selects the word.
       (A drag with motion keeps the region, as pinned above.) */
    send_mouse (MOUSE_PRESS, 0, 3, 2);
    send_mouse (MOUSE_RELEASE, 0, 3, 2);	/* count 1 elsewhere */
    send_mouse (MOUSE_PRESS, 0, 3, 8);
    send_mouse (MOUSE_RELEASE, 0, 3, 7);	/* drift, no motion */
    assert (mouseselect == FALSE);
    assert_pos (&second_window.w_dot, short_line, 8);
    send_mouse (MOUSE_PRESS, 0, 3, 8);
    send_mouse (MOUSE_RELEASE, 0, 3, 8);	/* count 2: word */
    assert_pos (&second_window.w_mark, short_line, 6);
    assert_pos (&second_window.w_dot, short_line, 10);
    assert (mouseselect == TRUE);
    endmouseselect ();
  }

  /* Complete click gestures with a backend-supplied count. */
  reset_mouse_windows ();
  {
    /* A double click selects the word at the clicked cell. */
    send_click (0, 3, 8, 2);
    assert_pos (&second_window.w_mark, short_line, 6);
    assert_pos (&second_window.w_dot, short_line, 10);
    assert (mouseselect == TRUE);

    /* A triple click selects the whole clicked line. */
    send_click (0, 3, 4, 3);
    assert_pos (&second_window.w_mark, short_line, 0);
    assert_pos (&second_window.w_dot, short_line, 10);

    /* A single click just moves the point. */
    send_click (0, 3, 2, 1);
    assert_pos (&second_window.w_dot, short_line, 2);
    assert (mouseselect == FALSE);

    /* A click on a mode line selects that window. */
    send_click (0, 2, 0, 1);
    assert (curwp == &window);
    send_click (0, 5, 0, 1);
    assert (curwp == &second_window);
  }

  /* A middle click yanks; a right click extends the region. */
  reset_mouse_windows ();
  {
    yank_calls = 0;
    send_click (1, 3, 4, 1);
    assert (yank_calls == 1);
    assert_pos (&second_window.w_dot, short_line, 4);
    assert (mouseselect == FALSE);

    /* A slow middle press yanks too, and its release is ignored. */
    send_mouse (MOUSE_PRESS, 1, 0, 4);
    assert (yank_calls == 2);
    assert_pos (&window.w_dot, long_line, 4);
    send_mouse (MOUSE_RELEASE, 1, 0, 4);
    assert (yank_calls == 2);

    /* With a mark set, a right click extends the region to the
       clicked cell. */
    second_window.w_mark.p = short_line;
    second_window.w_mark.o = 2;
    mouseselect = TRUE;
    send_click (2, 3, 9, 1);
    assert_pos (&second_window.w_dot, short_line, 9);
    assert_pos (&second_window.w_mark, short_line, 2);
    assert (mouseselect == TRUE);

    /* A right press extends as well. */
    send_mouse (MOUSE_PRESS, 2, 3, 4);
    assert_pos (&second_window.w_dot, short_line, 4);
    assert_pos (&second_window.w_mark, short_line, 2);

    /* Without a mark it just moves the point. */
    second_window.w_mark.p = NULL;
    endmouseselect ();
    send_click (2, 3, 6, 1);
    assert_pos (&second_window.w_dot, short_line, 6);
    assert (mouseselect == FALSE);
  }

  /* A press on a mode line selects that window. */
  reset_mouse_windows ();
  {
    mouseselect = TRUE;
    send_mouse (MOUSE_PRESS, 0, 5, 0);
    assert (curwp == &second_window);
    assert (mouseselect == FALSE);
    send_mouse (MOUSE_RELEASE, 0, 5, 0);
    assert (curwp == &second_window);
    send_mouse (MOUSE_PRESS, 0, 2, 0);
    assert (curwp == &window);
  }

  /* A shifted wheel scrolls horizontally instead of vertically. */
  reset_mouse_windows ();
  {
    int i;

    send_mouse_mods (MOUSE_WHEEL, 4, 0, 0, MOUSE_SHIFT);
    assert (curwp == &window);
    assert (window.w_leftcol == 4);
    assert (window.w_hscroll == TRUE);
    assert ((window.w_flag & WFHARD) != 0);

    /* Right scrolling stops while the widest visible line (30
       columns) still has text to show. */
    for (i = 0; i < 7; ++i)
      send_mouse_mods (MOUSE_WHEEL, 4, 0, 0, MOUSE_SHIFT);
    assert (window.w_leftcol == 28);
    send_mouse_mods (MOUSE_WHEEL, 4, 0, 0, MOUSE_SHIFT);
    assert (window.w_leftcol == 28);	/* clamped */

    /* Wheel up scrolls back and stops at the left edge. */
    for (i = 0; i < 8; ++i)
      send_mouse_mods (MOUSE_WHEEL, 3, 0, 0, MOUSE_SHIFT);
    assert (window.w_leftcol == 0);
    send_mouse_mods (MOUSE_WHEEL, 3, 0, 0, MOUSE_SHIFT);
    assert (window.w_leftcol == 0);	/* clamped */

    /* The second window scrolls by its own widest line (10). */
    send_mouse_mods (MOUSE_WHEEL, 4, 3, 0, MOUSE_SHIFT);
    assert (curwp == &second_window);
    assert (second_window.w_leftcol == 4);
    send_mouse_mods (MOUSE_WHEEL, 4, 3, 0, MOUSE_SHIFT);
    assert (second_window.w_leftcol == 8);
    send_mouse_mods (MOUSE_WHEEL, 4, 3, 0, MOUSE_SHIFT);
    assert (second_window.w_leftcol == 8);	/* clamped */

    /* An unshifted wheel still scrolls vertically. */
    mvupwind_calls = 0;
    send_mouse (MOUSE_WHEEL, 3, 0, 0);
    assert (mvupwind_calls == 1);
    assert (window.w_leftcol == 0);	/* untouched */

    /* A widest line of wide characters counts display columns: ten
       wide characters are twenty columns, so scrolling stops four
       columns short of twenty. */
    cjk_line = make_line ("\xe4\xb8\x80\xe4\xba\x8c\xe4\xb8\x89"
                          "\xe5\x9b\x9b\xe4\xba\x94\xe5\x85\xad"
                          "\xe4\xb8\x83\xe5\x85\xab\xe4\xb9\x9d"
                          "\xe5\x8d\x81");
    long_line->l_fp = cjk_line;
    cjk_line->l_bp = long_line;
    cjk_line->l_fp = short_line;
    short_line->l_bp = cjk_line;
    window.w_linep = cjk_line;
    window.w_dot.p = cjk_line;
    window.w_leftcol = 0;
    for (i = 0; i < 5; ++i)
      send_mouse_mods (MOUSE_WHEEL, 4, 0, 0, MOUSE_SHIFT);
    assert (window.w_leftcol == 16);
    send_mouse_mods (MOUSE_WHEEL, 4, 0, 0, MOUSE_SHIFT);
    assert (window.w_leftcol == 16);	/* clamped */
  }

  /* Legacy X10 reports carry the same button bits as SGR, so the X10
     decoder in the tty layer (mouse_decode_x10) runs them through
     mousedecodesgr: until a release byte is seen, a press byte is
     queued as one complete click, because X10 reports no releases of
     its own, and consecutive presses near one cell are counted; once
     one release byte (button bits 3, the mode 1000 encoding, which
     dvtm re-encodes clicks with) has arrived, presses are queued as
     plain presses and the core anchors drags on them and counts
     clicks on the releases, as for SGR.  The first release carries
     the count of the presses already reported as clicks, which the
     core adopts so the streak continues.  Wheel and motion bytes
     are single events that break the count.  The tty layer is not
     linked into this test, so pin that calling pattern here.  */
  reset_mouse_windows ();
  {
    EMOUSE event;

    /* The shifted-wheel block above leaves the second window
       scrolled; clicks there are text columns of the view. */
    second_window.w_leftcol = 0;
    second_window.w_mark.p = NULL;

    /* A wheel byte decodes with its modifier bits, as an X10 terminal
       sends Shift+wheel (button code 68). */
    assert (mousedecodesgr (&event, 68, 3, 8, FALSE) == TRUE);
    assert (event.kind == MOUSE_WHEEL && event.button == 3);
    assert (event.modifiers == MOUSE_SHIFT && event.clicks == 0);

    /* The decoder re-tags a button-3 press as a release of button 0,
       because the byte names no button; without a matching press
       the core's release handler must not disturb the click count
       or a fresh selection. */
    assert (mousedecodesgr (&event, 3, 3, 8, FALSE) == TRUE);
    assert (event.kind == MOUSE_PRESS && event.button == 3);
    event.kind = MOUSE_RELEASE;
    event.button = 0;
    event.clicks = 0;
    assert (mouseenqueue (&event) == TRUE);
    assert (mouseevent (FALSE, 1, 0) == TRUE);
    assert (mouseselect == FALSE);

    /* One X10 press is one click gesture with the count: 1 moves the
       point, 2 selects the word, 3 the line. */
    send_click (0, 3, 8, 1);
    assert_pos (&second_window.w_dot, short_line, 8);
    assert (second_window.w_mark.p == NULL);
    send_click (0, 3, 8, 2);
    assert_pos (&second_window.w_mark, short_line, 6);
    assert_pos (&second_window.w_dot, short_line, 10);
    send_click (0, 3, 8, 3);
    assert_pos (&second_window.w_mark, short_line, 0);

    /* A press on another cell starts the count over. */
    send_click (0, 3, 2, 1);
    assert (mouseselect == FALSE);
    send_click (0, 3, 8, 2);
    assert_pos (&second_window.w_mark, short_line, 6);

    /* A wheel byte between presses breaks the count too. */
    send_mouse_mods (MOUSE_WHEEL, 4, 3, 8, 0);
    send_click (0, 3, 8, 1);
    assert (mouseselect == FALSE);
    assert_pos (&second_window.w_dot, short_line, 8);

    /* The first release of a terminal that reports them carries the
       count of the presses already reported as whole clicks; the
       core adopts it, so the next release of the streak counts as
       the word-selecting second click. */
    send_click (0, 3, 8, 1);		/* press queued as a click */
    assert (mouseselect == FALSE);
    memset (&event, 0, sizeof (event));
    event.kind = MOUSE_RELEASE;		/* first release: count 1 */
    event.button = 0;
    event.row = 3;
    event.column = 8;
    event.clicks = 1;
    assert (mouseenqueue (&event) == TRUE);
    assert (mouseevent (FALSE, 1, 0) == TRUE);
    send_mouse (MOUSE_PRESS, 0, 3, 8);	/* now a plain press */
    send_mouse (MOUSE_RELEASE, 0, 3, 8); /* count 2: word select */
    assert_pos (&second_window.w_mark, short_line, 6);
    assert_pos (&second_window.w_dot, short_line, 10);
    assert (mouseselect == TRUE);

    /* After the switch to plain presses, a press/motion/release
       drag extends the region, as for SGR: the mark stays on the
       pressed cell and the point follows the drag. */
    send_mouse (MOUSE_PRESS, 0, 3, 8);
    send_mouse (MOUSE_MOTION, 0, 3, 5);
    send_mouse (MOUSE_RELEASE, 0, 3, 5);
    assert_pos (&second_window.w_mark, short_line, 8);
    assert_pos (&second_window.w_dot, short_line, 5);
    assert (mouseselect == TRUE);
    endmouseselect ();

    /* A drag as the very first gesture of a release-reporting
       terminal: the press was reported as one whole click before
       any release had been seen, so the release completes the
       region from the clicked cell to the released cell. */
    reset_mouse_windows ();
    second_window.w_leftcol = 0;
    send_click (0, 3, 5, 1);		/* press reported as a click */
    assert (mouseselect == FALSE);
    memset (&event, 0, sizeof (event));
    event.kind = MOUSE_RELEASE;		/* its release, far away */
    event.button = 0;
    event.row = 3;
    event.column = 8;
    event.clicks = 1;
    assert (mouseenqueue (&event) == TRUE);
    assert (mouseevent (FALSE, 1, 0) == TRUE);
    assert_pos (&second_window.w_mark, short_line, 5);
    assert_pos (&second_window.w_dot, short_line, 8);
    assert (mouseselect == TRUE);
    endmouseselect ();

    /* The same release near the clicked cell stays a settling
       click, not a two-cell drag. */
    send_click (0, 3, 5, 1);
    assert_pos (&second_window.w_dot, short_line, 5);
    memset (&event, 0, sizeof (event));
    event.kind = MOUSE_RELEASE;
    event.button = 0;
    event.row = 3;
    event.column = 6;			/* one cell of drift */
    event.clicks = 1;
    assert (mouseenqueue (&event) == TRUE);
    assert (mouseevent (FALSE, 1, 0) == TRUE);
    assert (mouseselect == FALSE);
  }

  /* Selection highlight painting. */
  reset_mouse_windows ();
  {
    int i;

    ncol = 80;
    memset (&row, 0, sizeof (row));
    vscreen[0] = &row;
    vttextmove (0, 0, 0);
    for (i = 0; i < NCOL; ++i)
      row.v_attr[i] = CTEXT;

    /* Nothing without the highlight, and nothing for an empty
       region. */
    window.w_mark.p = long_line;
    window.w_mark.o = 4;
    window.w_dot.p = long_line;
    window.w_dot.o = 4;
    mouseselect = FALSE;
    assert (selection_setup (&window, long_line) == FALSE);
    mouseselect = TRUE;
    assert (selection_setup (&window, long_line) == FALSE);

    /* A single-line region paints its own columns only. */
    window.w_mark.o = 6;
    window.w_dot.o = 8;
    assert (selection_setup (&window, long_line) == TRUE);
    draw_selection (long_line);
    assert (row.v_attr[5] == CTEXT);
    assert (row.v_attr[6] == CSELECT);
    assert (row.v_attr[7] == CSELECT);
    assert (row.v_attr[8] == CTEXT);

    /* A two-line region paints the tail of the first line and the
       head of the last. */
    window.w_mark.p = long_line;
    window.w_mark.o = 2;
    window.w_dot.p = short_line;
    window.w_dot.o = 3;
    for (i = 0; i < NCOL; ++i)
      row.v_attr[i] = CTEXT;
    assert (selection_setup (&window, long_line) == TRUE);
    draw_selection (long_line);
    assert (row.v_attr[1] == CTEXT);
    assert (row.v_attr[2] == CSELECT);
    assert (row.v_attr[29] == CSELECT);
    assert (row.v_attr[30] == CTEXT);
    for (i = 0; i < NCOL; ++i)
      row.v_attr[i] = CTEXT;
    draw_selection (short_line);
    assert (row.v_attr[0] == CSELECT);
    assert (row.v_attr[2] == CSELECT);
    assert (row.v_attr[3] == CTEXT);

    /* Swapping mark and point paints the same cells. */
    window.w_mark.p = short_line;
    window.w_mark.o = 3;
    window.w_dot.p = long_line;
    window.w_dot.o = 2;
    for (i = 0; i < NCOL; ++i)
      row.v_attr[i] = CTEXT;
    assert (selection_setup (&window, long_line) == TRUE);
    draw_selection (long_line);
    assert (row.v_attr[2] == CSELECT);
    assert (row.v_attr[29] == CSELECT);
    for (i = 0; i < NCOL; ++i)
      row.v_attr[i] = CTEXT;
    draw_selection (short_line);
    assert (row.v_attr[0] == CSELECT);
    assert (row.v_attr[3] == CTEXT);
    mouseselect = FALSE;
  }

  free (long_line);
  free (short_line);

  puts ("display tests passed");
  return 0;
}
