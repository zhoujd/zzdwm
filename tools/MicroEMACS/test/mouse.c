/*
 * Tests for mouse click column mapping (mouse_col_to_offset in
 * display.c) and for the mouseevent drag state machine, included
 * directly so the shared display code is exercised with the same
 * configuration as display-test.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../display.c"

/*
 * Build a LINE from UTF-8 text.  The caller frees the line.
 */
static LINE *
make_line (const char *text)
{
  size_t len = strlen (text);
  LINE *lp = calloc (1, LINEHDR_SIZE + len + 1);

  assert (lp != NULL);
  lp->l_size = len + 1;
  lp->l_used = len;
  memcpy (lp->l_text, text, len);
  return lp;
}

/*
 * Minimal editor state to drive mouseevent: globals that main.c
 * owns in the real editor, and a stub for the one window routine
 * that lives outside display.c.  updatemode is compiled in from
 * display.c itself.
 */
BUFFER *curbp;
EWINDOW *curwp;
EWINDOW *wheadp;
int ncol = 80;                  /* Console width, for the gutter. */
int numberflag;                 /* Line-number mode is off.       */

static int mvupwind_calls;
static int mvupwind_last_n;

int
mvupwind (int f, int n, int k)
{
  (void) f;
  (void) k;
  mvupwind_calls++;
  mvupwind_last_n = n;
  return TRUE;
}

static BUFFER test_buf;
static EWINDOW test_win;
static LINE *test_line1;
static LINE *test_line2;
static LINE *test_line3;
static LINE *test_end;

/*
 * One window, rows 0..9 text and row 10 the mode line, over a
 * buffer of three lines.  Buffer line order is circular around the
 * empty end-of-buffer line, as ldelete and friends expect.
 */
static void
make_window (void)
{
  wheadp = &test_win;
  curwp = &test_win;
  curbp = &test_buf;
  memset (&test_win, 0, sizeof (test_win));
  memset (&test_buf, 0, sizeof (test_buf));
  test_win.w_bufp = &test_buf;
  test_win.w_toprow = 0;
  test_win.w_ntrows = 10;
  test_buf.b_linep = test_end = make_line ("");
  test_win.w_linep = test_line1 = make_line ("alpha beta");
  test_line2 = make_line ("gamma delta");
  test_line3 = make_line ("epsilon");
  test_line1->l_fp = test_line2;
  test_line2->l_bp = test_line1;
  test_line2->l_fp = test_line3;
  test_line3->l_bp = test_line2;
  test_line3->l_fp = test_end;
  test_end->l_bp = test_line3;
  test_end->l_fp = test_line1;
  test_line1->l_bp = test_end;
  test_win.w_dot.p = test_line1;
  test_win.w_dot.o = 0;
  test_win.w_mark.p = NULL;
  test_win.w_mark.o = 0;
}

static void
free_window (void)
{
  free (test_line1);
  free (test_line2);
  free (test_line3);
  free (test_end);
}

/*
 * Feed one event to mouseevent and require success.
 */
static void
send_mouse (int kind, int button, int row, int col)
{
  mouse_kind = kind;
  mouse_button = button;
  mouse_row = row;
  mouse_column = col;
  mouse_mods = 0;
  assert (mouseevent (FALSE, 1, KRANDOM) == TRUE);
}

static void
assert_pos (POS * pp, LINE * lp, int o)
{
  assert (pp->p == lp);
  assert (pp->o == o);
}

static void
test_click_mapping (void)
{
  LINE *lp;

  /* Plain ASCII: clicks land on the exact column. */
  lp = make_line ("hello");
  assert (mouse_col_to_offset (lp, 0) == 0);
  assert (mouse_col_to_offset (lp, 3) == 3);
  assert (mouse_col_to_offset (lp, 99) == 5);    /* clamp to end */
  assert (mouse_col_to_offset (lp, -1) == 0);    /* clamp to start */
  free (lp);

  /* Two-byte UTF-8 characters occupy one column. */
  lp = make_line ("a\303\251b");                 /* a é b */
  assert (mouse_col_to_offset (lp, 1) == 1);
  assert (mouse_col_to_offset (lp, 2) == 2);
  free (lp);

  /* Wide characters occupy two columns and cannot be split:
     clicking either half stops before the character. */
  lp = make_line ("a\346\274\242b");             /* a 汉 b */
  assert (mouse_col_to_offset (lp, 1) == 1);     /* left half of 汉 */
  assert (mouse_col_to_offset (lp, 2) == 1);     /* right half of 汉 */
  assert (mouse_col_to_offset (lp, 3) == 2);     /* on the b         */
  assert (mouse_col_to_offset (lp, 4) == 3);     /* past the b       */
  free (lp);

  /* Empty lines clamp to zero. */
  lp = make_line ("");
  assert (mouse_col_to_offset (lp, 4) == 0);
  free (lp);
}

static void
test_drag_with_motion (void)
{
  make_window ();

  /* Press anchors the drag: point and mark land together. */
  send_mouse (MOUSE_PRESS, 0, 0, 6);
  assert_pos (&test_win.w_dot, test_line1, 6);
  assert_pos (&test_win.w_mark, test_line1, 6);

  /* Motion extends the region by moving the point only. */
  send_mouse (MOUSE_MOTION, 0, 2, 5);
  assert_pos (&test_win.w_dot, test_line3, 5);
  assert_pos (&test_win.w_mark, test_line1, 6);

  /* Release keeps the region. */
  send_mouse (MOUSE_RELEASE, 0, 2, 20);
  assert_pos (&test_win.w_dot, test_line3, 7);   /* clamp to line end */
  assert_pos (&test_win.w_mark, test_line1, 6);

  /* Idle motion after the release moves nothing. */
  send_mouse (MOUSE_MOTION, 0, 1, 3);
  assert_pos (&test_win.w_dot, test_line3, 7);

  free_window ();
}

static void
test_drag_without_motion (void)
{
  make_window ();
  test_win.w_mark.p = NULL;

  /* Terminals that report no motion while dragging: the press
     anchors the mark, and the release position completes the
     region.  The mark must exist after the gesture. */
  send_mouse (MOUSE_PRESS, 0, 1, 6);
  send_mouse (MOUSE_RELEASE, 0, 2, 5);
  assert_pos (&test_win.w_dot, test_line3, 5);
  assert_pos (&test_win.w_mark, test_line2, 6);

  free_window ();
}

static void
test_click_restores_mark (void)
{
  make_window ();

  /* A one-event click must not disturb an existing mark. */
  test_win.w_mark.p = test_line3;
  test_win.w_mark.o = 3;
  send_mouse (MOUSE_CLICK, 0, 1, 4);
  assert_pos (&test_win.w_dot, test_line2, 4);
  assert_pos (&test_win.w_mark, test_line3, 3);

  /* A press and release at the same cell is a plain click too:
     the mark before the gesture goes back. */
  test_win.w_mark.p = NULL;
  send_mouse (MOUSE_PRESS, 0, 2, 2);
  assert_pos (&test_win.w_mark, test_line3, 2);
  send_mouse (MOUSE_RELEASE, 0, 2, 2);
  assert_pos (&test_win.w_dot, test_line3, 2);
  assert (test_win.w_mark.p == NULL);

  /* A press and release at different cells without motion
     reports is a drag: the mark stays. */
  send_mouse (MOUSE_PRESS, 0, 0, 2);
  send_mouse (MOUSE_RELEASE, 0, 1, 2);
  assert_pos (&test_win.w_dot, test_line2, 2);
  assert_pos (&test_win.w_mark, test_line1, 2);

  free_window ();
}

static void
test_click_ends_drag (void)
{
  make_window ();
  test_win.w_mark.p = NULL;

  /* A click whose release the tty layer folded into the press
     must end the drag: idle motion afterwards moves nothing. */
  send_mouse (MOUSE_PRESS, 0, 0, 1);
  assert (test_win.w_mark.p != NULL);
  send_mouse (MOUSE_CLICK, 0, 1, 1);
  assert_pos (&test_win.w_dot, test_line2, 1);
  send_mouse (MOUSE_MOTION, 0, 2, 3);
  assert_pos (&test_win.w_dot, test_line2, 1);   /* still idle */

  free_window ();
}

static void
test_modeline_click (void)
{
  EWINDOW win2;
  BUFFER buf2;
  LINE *end2;
  LINE *line2;

  make_window ();

  /* A second window, rows 6..9 text and row 10... arrange the
     first window to rows 0..3 with mode line row 4, the second to
     rows 5..9 with mode line row 10. */
  test_win.w_ntrows = 4;
  memset (&win2, 0, sizeof (win2));
  memset (&buf2, 0, sizeof (buf2));
  win2.w_bufp = &buf2;
  win2.w_toprow = 5;
  win2.w_ntrows = 5;
  buf2.b_linep = end2 = make_line ("");
  win2.w_linep = line2 = make_line ("second");
  line2->l_fp = end2;
  end2->l_bp = line2;
  end2->l_fp = line2;
  line2->l_bp = end2;
  win2.w_dot.p = line2;
  win2.w_dot.o = 0;
  win2.w_mark.p = NULL;
  test_win.w_wndp = &win2;

  /* Clicking the mode line of the second window selects it. */
  send_mouse (MOUSE_CLICK, 0, 10, 0);
  assert (curwp == &win2);
  assert (curbp == &buf2);
  assert_pos (&win2.w_dot, line2, 0);   /* point did not move */

  /* Clicking its text moves the point there. */
  send_mouse (MOUSE_CLICK, 0, 6, 3);
  assert (curwp == &win2);
  assert_pos (&win2.w_dot, line2, 3);

  /* A drag stays in the window it started in: press in the second
     window and release over the first. */
  win2.w_mark.p = NULL;
  send_mouse (MOUSE_PRESS, 0, 7, 2);
  send_mouse (MOUSE_RELEASE, 0, 1, 2);
  assert (curwp == &win2);
  assert_pos (&win2.w_dot, line2, 2);   /* never left win2 */
  assert (win2.w_mark.p == NULL);       /* plain click after all */

  free (end2);
  free (line2);
  free_window ();
}

static void
test_wheel (void)
{
  make_window ();

  send_mouse (MOUSE_WHEEL, 3, 1, 0);
  assert (mvupwind_calls == 1);
  assert (mvupwind_last_n == mouse_wheel_step);
  send_mouse (MOUSE_WHEEL, 4, 1, 0);
  assert (mvupwind_calls == 2);
  assert (mvupwind_last_n == -mouse_wheel_step);

  /* The step is a Lisp variable; mouseevent must read it live. */
  mouse_wheel_step = 7;
  send_mouse (MOUSE_WHEEL, 3, 1, 0);
  assert (mvupwind_last_n == 7);
  mouse_wheel_step = 3;

  free_window ();
}

int
main (void)
{
  test_click_mapping ();
  test_drag_with_motion ();
  test_drag_without_motion ();
  test_click_restores_mark ();
  test_click_ends_drag ();
  test_modeline_click ();
  test_wheel ();
  puts ("mouse tests passed");
  return 0;
}
