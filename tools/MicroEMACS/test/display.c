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
    MOUSE_EVENT event;

    memset (&event, 0, sizeof (event));
    event.kind = MOUSE_PRESS;
    event.button = 0;
    event.row = 4;
    event.column = 6;
    assert (mouseenqueue (&event) == TRUE);
    assert (mouseevent (FALSE, 1, 0) == TRUE);
    assert (curwp == &second_window);
    assert (curbp == &buffer);
    assert (second_window.w_dot.p == short_line);
    assert (second_window.w_dot.o == 6);
    assert (second_window.w_hscroll == FALSE);
    assert ((second_window.w_flag & WFMOVE) != 0);

    event.row = 1;
    event.column = 30;
    assert (mouseenqueue (&event) == TRUE);
    assert (mouseevent (FALSE, 1, 0) == TRUE);
    assert (curwp == &window);
    assert (window.w_dot.p == short_line);
    assert (window.w_dot.o == wllength (short_line));
  }

  free (long_line);
  free (short_line);

  puts ("display tests passed");
  return 0;
}
