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
int tabsize = 8;                /* Tab width, for selection cols. */

static int mvupwind_calls;
static int mvupwind_last_n;
static int yank_calls;

int
mvupwind (int f, int n, int k)
{
  (void) f;
  (void) k;
  mvupwind_calls++;
  mvupwind_last_n = n;
  return TRUE;
}

int
yank (int f, int n, int k)
{
  (void) f;
  (void) n;
  (void) k;
  yank_calls++;
  return TRUE;
}

/*
 * Capture clipboard copies for osc52send instead of writing an
 * escape sequence to the test terminal.
 */
static char last_osc52[512];
static int osc52_calls;

void
ttosc52 (const char *b64)
{
  ++osc52_calls;
  snprintf (last_osc52, sizeof (last_osc52), "%s", b64);
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
send_mouse_mods (int kind, int button, int row, int col, int mods)
{
  mouse_kind = kind;
  mouse_button = button;
  mouse_row = row;
  mouse_column = col;
  mouse_mods = mods;
  mouse_clicks = 0;
  assert (mouseevent (FALSE, 1, KRANDOM) == TRUE);
}

static void
send_mouse (int kind, int button, int row, int col)
{
  send_mouse_mods (kind, button, row, col, 0);
}

/*
 * Feed a completed click gesture with a click count.
 */
static void
send_click (int button, int row, int col, int clicks)
{
  mouse_kind = MOUSE_CLICK;
  mouse_button = button;
  mouse_row = row;
  mouse_column = col;
  mouse_mods = 0;
  mouse_clicks = clicks;
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

  /* The highlight stays on until a non-mouse key. */
  assert (mouseselect == TRUE);
  endmouseselect ();
  assert (mouseselect == FALSE);
  assert ((test_win.w_flag & WFHARD) != 0);      /* repaint asked */

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
  assert (mouseselect == FALSE);         /* click ends highlight */

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

static void
test_shift_wheel (void)
{
  int before = mvupwind_calls;

  make_window ();

  /* Shift+wheel-down steps right in columns of four, clamped so
     the view never runs past the widest visible line (11 chars
     here: "gamma delta"). */
  send_mouse_mods (MOUSE_WHEEL, 4, 1, 0, MOUSE_SHIFT);
  assert (test_win.w_leftcol == 4);
  assert (test_win.w_hscroll != 0);
  send_mouse_mods (MOUSE_WHEEL, 4, 1, 0, MOUSE_SHIFT);
  assert (test_win.w_leftcol == 8);
  send_mouse_mods (MOUSE_WHEEL, 4, 1, 0, MOUSE_SHIFT);
  assert (test_win.w_leftcol == 8);	/* 8 + 4 >= 11: stop */
  assert (mvupwind_calls == before);	/* never a vertical scroll */
  assert_pos (&test_win.w_dot, test_line1, 0);  /* dot untouched */
  assert ((test_win.w_flag & WFHARD) != 0);

  /* Wheel-up steps back and clamps at the left edge. */
  send_mouse_mods (MOUSE_WHEEL, 3, 1, 0, MOUSE_SHIFT);
  assert (test_win.w_leftcol == 4);
  send_mouse_mods (MOUSE_WHEEL, 3, 1, 0, MOUSE_SHIFT);
  send_mouse_mods (MOUSE_WHEEL, 3, 1, 0, MOUSE_SHIFT);
  send_mouse_mods (MOUSE_WHEEL, 3, 1, 0, MOUSE_SHIFT);
  assert (test_win.w_leftcol == 0);

  free_window ();
}

/* Deterministic clock for the SGR click-count window. */
static long fake_clock_time;
static long
fake_clock (void)
{
  return fake_clock_time;
}

static void
test_sgr_decode (void)
{
  long (*real_clock) (void) = mouse_clock;

  mouse_clock = fake_clock;
  fake_clock_time = 100;

  /* Press and release at one cell is a click; repeats count up. */
  mouse_decode_sgr (0, 1, 4, FALSE);
  assert (mouse_kind == MOUSE_PRESS && mouse_button == 0);
  assert (mouse_row == 1 && mouse_column == 4);
  mouse_decode_sgr (0, 1, 4, TRUE);
  assert (mouse_kind == MOUSE_CLICK && mouse_clicks == 1);
  mouse_decode_sgr (0, 1, 4, FALSE);
  mouse_decode_sgr (0, 1, 4, TRUE);
  assert (mouse_kind == MOUSE_CLICK && mouse_clicks == 2);
  mouse_decode_sgr (0, 1, 4, FALSE);
  mouse_decode_sgr (0, 1, 4, TRUE);
  assert (mouse_kind == MOUSE_CLICK && mouse_clicks == 3);

  /* A wheel between clicks starts a fresh gesture.  The clock jump
     also expires the previous gesture, so the pair below counts 1. */
  fake_clock_time = 200;
  mouse_decode_sgr (0, 1, 4, FALSE);
  mouse_decode_sgr (0, 1, 4, TRUE);
  assert (mouse_kind == MOUSE_CLICK && mouse_clicks == 1);
  mouse_decode_sgr (64, 1, 4, FALSE);
  mouse_decode_sgr (0, 1, 4, FALSE);
  mouse_decode_sgr (0, 1, 4, TRUE);
  assert (mouse_kind == MOUSE_CLICK && mouse_clicks == 1);

  /* Motion during the press makes the release a plain release. */
  mouse_decode_sgr (0, 2, 2, FALSE);
  mouse_decode_sgr (32, 2, 5, FALSE);
  assert (mouse_kind == MOUSE_MOTION && mouse_button == 0);
  mouse_decode_sgr (0, 2, 5, TRUE);
  assert (mouse_kind == MOUSE_RELEASE && mouse_clicks == 0);
  mouse_decode_sgr (0, 2, 5, TRUE);   /* stray release: no gesture */
  assert (mouse_kind == MOUSE_RELEASE);

  /* Clicks too far apart do not count up. */
  fake_clock_time = 1000;
  mouse_decode_sgr (0, 1, 4, FALSE);
  mouse_decode_sgr (0, 1, 4, TRUE);
  assert (mouse_kind == MOUSE_CLICK && mouse_clicks == 1);

  /* Wheel and modifier decoding, Shift+wheel above all. */
  mouse_decode_sgr (69, 1, 4, FALSE);
  assert (mouse_kind == MOUSE_WHEEL && mouse_button == 4);
  assert (mouse_mods == MOUSE_SHIFT);
  mouse_decode_sgr (68, 1, 4, FALSE);
  assert (mouse_kind == MOUSE_WHEEL && mouse_button == 3);
  assert (mouse_mods == MOUSE_SHIFT);
  mouse_decode_sgr (64, 1, 4, FALSE);
  assert (mouse_kind == MOUSE_WHEEL && mouse_button == 3);
  assert (mouse_mods == 0);
  mouse_decode_sgr (16, 1, 4, FALSE);
  assert (mouse_kind == MOUSE_PRESS && mouse_mods == MOUSE_CTRL);
  mouse_decode_sgr (2 | 8, 1, 4, FALSE);
  assert (mouse_kind == MOUSE_PRESS && mouse_button == 2);
  assert (mouse_mods == MOUSE_ALT);

  /* A release at a different cell is a plain release. */
  fake_clock_time = 2000;
  mouse_decode_sgr (0, 1, 1, FALSE);
  mouse_decode_sgr (0, 1, 6, TRUE);
  assert (mouse_kind == MOUSE_RELEASE);

  mouse_clock = real_clock;
}

static void
test_selection_highlight (void)
{
  VIDEO *vrow;
  int i;

  make_window ();
  vrow = calloc (1, sizeof (VIDEO));
  assert (vrow != NULL);
  vscreen[0] = vrow;
  vttextmove (0, 0, 0);

  /* Region from (line1,6) to (line3,3). */
  test_win.w_mark.p = test_line1;
  test_win.w_mark.o = 6;
  test_win.w_dot.p = test_line3;
  test_win.w_dot.o = 3;
  mouseselect = TRUE;

#define CLEAR_ATTRS() do { for (i = 0; i < NCOL; ++i) vrow->v_attr[i] = CTEXT; } while (0)

  /* First line: columns 6..9 selected ("alpha beta" is 10 wide). */
  CLEAR_ATTRS ();
  assert (selection_setup (&test_win, test_line1) == TRUE);
  draw_selection (test_line1);
  assert (vrow->v_attr[5] == CTEXT);
  assert (vrow->v_attr[6] == CSELECT);
  assert (vrow->v_attr[9] == CSELECT);
  assert (vrow->v_attr[10] == CTEXT);

  /* Middle line: fully selected ("gamma delta" is 11 wide). */
  CLEAR_ATTRS ();
  assert (selection_setup (&test_win, test_line2) == TRUE);
  draw_selection (test_line2);
  assert (vrow->v_attr[0] == CSELECT);
  assert (vrow->v_attr[10] == CSELECT);
  assert (vrow->v_attr[11] == CTEXT);

  /* Last line: columns 0..2 selected. */
  CLEAR_ATTRS ();
  assert (selection_setup (&test_win, test_line1) == TRUE);
  draw_selection (test_line3);
  assert (vrow->v_attr[0] == CSELECT);
  assert (vrow->v_attr[2] == CSELECT);
  assert (vrow->v_attr[3] == CTEXT);

  /* Swapping mark and point paints the same cells. */
  test_win.w_mark.p = test_line3;
  test_win.w_mark.o = 3;
  test_win.w_dot.p = test_line1;
  test_win.w_dot.o = 6;
  CLEAR_ATTRS ();
  assert (selection_setup (&test_win, test_line1) == TRUE);
  draw_selection (test_line1);
  assert (vrow->v_attr[6] == CSELECT);
  assert (vrow->v_attr[10] == CTEXT);

  /* Empty region and highlight off draw nothing. */
  test_win.w_mark = test_win.w_dot;
  CLEAR_ATTRS ();
  assert (selection_setup (&test_win, test_line1) == FALSE);
  draw_selection (test_line1);
  assert (vrow->v_attr[6] == CTEXT);
  test_win.w_mark.p = test_line1;
  test_win.w_mark.o = 6;
  mouseselect = FALSE;
  CLEAR_ATTRS ();
  assert (selection_setup (&test_win, test_line1) == FALSE);
  draw_selection (test_line1);
  assert (vrow->v_attr[6] == CTEXT);

  /* A region on a single line (double click on a word) must not
     leak onto the lines below it: walk the rows like update does,
     one setup and a draw/step per line. */
  test_win.w_mark.p = test_line1;
  test_win.w_mark.o = 6;
  test_win.w_dot.p = test_line1;
  test_win.w_dot.o = 10;
  mouseselect = TRUE;
  CLEAR_ATTRS ();
  assert (selection_setup (&test_win, test_line1) == TRUE);
  draw_selection (test_line1);
  selection_step (test_line1);
  assert (vrow->v_attr[6] == CSELECT);
  assert (vrow->v_attr[9] == CSELECT);
  assert (vrow->v_attr[10] == CTEXT);
  CLEAR_ATTRS ();
  draw_selection (test_line2);
  selection_step (test_line2);
  assert (vrow->v_attr[0] == CTEXT);
  draw_selection (test_line3);
  assert (vrow->v_attr[0] == CTEXT);

  /* The same walk with a window that starts inside the region
     still finds the end of it. */
  test_win.w_mark.p = test_line1;
  test_win.w_mark.o = 6;
  test_win.w_dot.p = test_line3;
  test_win.w_dot.o = 3;
  CLEAR_ATTRS ();
  assert (selection_setup (&test_win, test_line2) == TRUE);
  assert (select_inside == TRUE);
  draw_selection (test_line2);
  selection_step (test_line2);
  assert (vrow->v_attr[10] == CSELECT);
  assert (vrow->v_attr[11] == CTEXT);
  CLEAR_ATTRS ();
  draw_selection (test_line3);
  selection_step (test_line3);
  assert (vrow->v_attr[2] == CSELECT);
  assert (vrow->v_attr[3] == CTEXT);
  assert (select_inside == FALSE);
  CLEAR_ATTRS ();
  draw_selection (test_end);
  assert (vrow->v_attr[0] == CTEXT);      /* walk ended */

#undef CLEAR_ATTRS
  mouseselect = FALSE;
  vscreen[0] = NULL;
  free (vrow);
  free_window ();
}

static void
test_double_click (void)
{
  make_window ();

  /* Double click inside "beta" selects the word. */
  send_click (0, 0, 8, 2);
  assert_pos (&test_win.w_dot, test_line1, 10);
  assert_pos (&test_win.w_mark, test_line1, 6);
  assert (mouseselect == TRUE);

  /* Double click on the space selects the blank run. */
  send_click (0, 0, 5, 2);
  assert_pos (&test_win.w_mark, test_line1, 5);
  assert_pos (&test_win.w_dot, test_line1, 6);

  /* A single click still just moves the point and leaves the mark
     alone. */
  send_click (0, 0, 2, 1);
  assert_pos (&test_win.w_dot, test_line1, 2);
  assert_pos (&test_win.w_mark, test_line1, 5);
  assert (mouseselect == FALSE);

  /* Past the end of the line there is no word to select. */
  send_click (0, 2, 20, 2);
  assert_pos (&test_win.w_dot, test_line3, 7);  /* clamped */
  assert (mouseselect == FALSE);

  free_window ();
}

static void
test_triple_click (void)
{
  make_window ();

  /* Triple click selects the whole clicked line. */
  send_click (0, 1, 4, 3);
  assert_pos (&test_win.w_mark, test_line2, 0);
  assert_pos (&test_win.w_dot, test_line2, 11);
  assert (mouseselect == TRUE);

  free_window ();
}

static void
test_middle_click (void)
{
  make_window ();
  test_win.w_mark.p = NULL;
  yank_calls = 0;

  /* A middle click yanks the kill buffer at the clicked cell. */
  send_click (1, 0, 3, 1);
  assert (yank_calls == 1);
  assert_pos (&test_win.w_dot, test_line1, 3);
  assert (mouseselect == FALSE);

  /* A middle press (a slow click) yanks too, and its release is
     ignored. */
  send_mouse (MOUSE_PRESS, 1, 0, 4);
  assert (yank_calls == 2);
  send_mouse (MOUSE_RELEASE, 1, 0, 4);
  assert (yank_calls == 2);

  free_window ();
}

static void
test_right_click (void)
{
  make_window ();

  /* With a mark set, the region extends to the clicked cell. */
  test_win.w_mark.p = test_line1;
  test_win.w_mark.o = 2;
  mouseselect = TRUE;
  send_click (2, 1, 9, 1);
  assert_pos (&test_win.w_dot, test_line2, 9);
  assert_pos (&test_win.w_mark, test_line1, 2);
  assert (mouseselect == TRUE);

  /* A right press extends as well. */
  send_mouse (MOUSE_PRESS, 2, 0, 4);
  assert_pos (&test_win.w_dot, test_line1, 4);
  assert_pos (&test_win.w_mark, test_line1, 2);

  /* Without a mark it just moves the point. */
  test_win.w_mark.p = NULL;
  endmouseselect ();
  send_click (2, 0, 6, 1);
  assert_pos (&test_win.w_dot, test_line1, 6);
  assert (mouseselect == FALSE);

  free_window ();
}

/*
 * Copy regions to the clipboard (osc52send in display.c): the text
 * between the region ends, in bytes with newlines between lines,
 * base64 encoded the way OSC 52 wants it.
 */
static void
send_region (LINE *lp, int o, int size)
{
  REGION region;

  region.r_pos.p = lp;
  region.r_pos.o = o;
  region.r_size = size;
  osc52send (&region);
}

static void
test_osc52 (void)
{
  LINE *lp;
  int calls;

  make_window ();
  assert (osc52 == 1);              /* on by default */
  osc52_calls = 0;

  /* Region across three lines, from mid-line into mid-line:
     "beta" + newline + "gamma delta" + newline + "eps". */
  send_region (test_line1, 6, 4 + 1 + 11 + 1 + 3);
  assert (osc52_calls == 1);
  assert (strcmp (last_osc52, "YmV0YQpnYW1tYSBkZWx0YQplcHM=") == 0);

  /* Region that ends exactly at the end of a line adds no
     trailing newline. */
  send_region (test_line1, 0, 10 + 1 + 11);
  assert (strcmp (last_osc52, "YWxwaGEgYmV0YQpnYW1tYSBkZWx0YQ==") == 0);

  /* Single line, one pad character. */
  send_region (test_line2, 0, 5);   /* "gamma" */
  assert (strcmp (last_osc52, "Z2FtbWE=") == 0);

  /* Two pad characters. */
  send_region (test_line1, 6, 4);   /* "beta" */
  assert (strcmp (last_osc52, "YmV0YQ==") == 0);

  /* UTF-8 characters are copied as bytes, not columns. */
  lp = make_line ("a\303\251b");    /* a é b */
  send_region (lp, 0, 3);
  assert (strcmp (last_osc52, "YcOpYg==") == 0);
  free (lp);

  /* The Lisp variable turns the copy off. */
  calls = osc52_calls;
  osc52 = 0;
  send_region (test_line2, 0, 5);
  assert (osc52_calls == calls);
  osc52 = 1;

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
  test_shift_wheel ();
  test_sgr_decode ();
  test_selection_highlight ();
  test_double_click ();
  test_triple_click ();
  test_middle_click ();
  test_right_click ();
  test_osc52 ();
  puts ("mouse tests passed");
  return 0;
}
