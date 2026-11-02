#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../display.c"

int ncol;
int tabsize;
int numberflag;

static VIDEO row;

static LINE *long_line;
static LINE *short_line;
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

void
eprintf (const char *format, ...)
{
  (void) format;
}

static int mvupwind_calls;
static int mvupwind_last_n;

int
mvupwind (int f, int n, int k)
{
  (void) f;
  (void) k;
  ++mvupwind_calls;
  mvupwind_last_n = n;
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
