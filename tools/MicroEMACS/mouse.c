/*
 * Filename mouse.c
 */

#include "def.h"

#ifdef MOUSE

#define MOUSE_QUEUE_SIZE 32

int mouseselect;			/* Region highlight is shown.	*/

static EMOUSE mouse_queue[MOUSE_QUEUE_SIZE];
static int mouse_queue_head;
static int mouse_queue_count;

void
mousereset (void)
{
  mouse_queue_head = 0;
  mouse_queue_count = 0;
}

int
mouseeventcount (void)
{
  return mouse_queue_count;
}

int
mouseenqueue (const EMOUSE *event)
{
  int tail;

  if (event == NULL || mouse_queue_count == MOUSE_QUEUE_SIZE)
    return FALSE;

  tail = (mouse_queue_head + mouse_queue_count) % MOUSE_QUEUE_SIZE;
  mouse_queue[tail] = *event;
  ++mouse_queue_count;
  return TRUE;
}

int
mousepeek (EMOUSE *event)
{
  if (event == NULL || mouse_queue_count == 0)
    return FALSE;

  *event = mouse_queue[mouse_queue_head];
  return TRUE;
}

int
mousedrop (EMOUSE *event)
{
  if (mousepeek (event) == FALSE)
    return FALSE;

  mouse_queue_head = (mouse_queue_head + 1) % MOUSE_QUEUE_SIZE;
  --mouse_queue_count;
  return TRUE;
}

int
mousedecodesgr (EMOUSE *event, int code, int row, int column,
                int release)
{
  if (event == NULL || code < 0 || code > 255 || row < 0 || column < 0)
    return FALSE;

  event->kind = release != FALSE ? MOUSE_RELEASE : MOUSE_PRESS;
  event->button = code & 3;
  event->row = row;
  event->column = column;
  event->modifiers = 0;
  event->clicks = 0;

  if ((code & 4) != 0)
    event->modifiers |= MOUSE_SHIFT;
  if ((code & 8) != 0)
    event->modifiers |= MOUSE_ALT;
  if ((code & 16) != 0)
    event->modifiers |= MOUSE_CTRL;

  if ((code & 64) != 0)
    {
      event->kind = MOUSE_WHEEL;
      event->button = (code & 1) != 0 ? 4 : 3;
    }
  else if ((code & 32) != 0)
    event->kind = MOUSE_MOTION;

  return TRUE;
}

int mouse_wheel_step = 3;	/* Lines per wheel scroll.	*/

static int
mouse_line_offset (const LINE *line, int column)
{
  const uchar *text = lgets (line);
  const uchar *end = text + llength (line);
  int display_column = 0;
  int offset = 0;

  while (text < end)
    {
      wchar_t character;
      int character_width;
      int length;

      character = ugetc (text, 0, &length);
      character_width = character == '\t'
                        ? tabsize - display_column % tabsize : uwidth (character);
      if (display_column >= column || display_column + character_width > column)
        return offset;

      display_column += character_width;
      text += length;
      ++offset;
    }
  return offset;
}

/*
 * Find the window that owns the given screen row, or NULL if the
 * row does not belong to a window (for example the echo line).
 * Windows are laid out top to bottom without gaps, so the first
 * window whose mode line is at or below the row owns it.
 */
static EWINDOW *
mouse_find_window (int row)
{
  EWINDOW *wp;

  ALLWIND (wp)
    {
      if (row < wp->w_toprow + wp->w_ntrows)
        return wp;		/* Text rows of this window. */
      if (row == wp->w_toprow + wp->w_ntrows)
        return wp;		/* Mode line of this window.  */
    }
  return NULL;
}

/*
 * Make the window under the mouse the current window.
 */
static void
mouse_switch_window (EWINDOW *wp)
{
  if (wp != curwp)
    {
      curwp = wp;
      curbp = wp->w_bufp;
      updatemode ();
    }
}

/*
 * Drag selection state: a button-0 press anchors the mark at the
 * pressed cell and the drag extends the region until the release.
 */
static int mouse_dragging;	/* Button 0 is held down.	 */
static int mouse_drag_moved;	/* Motion extended the region.	 */
static EWINDOW *mouse_anchor_wp; /* Window the drag started in.  */
static int mouse_anchor_row;	/* Pressed cell, row.		 */
static int mouse_anchor_col;	/* Pressed cell, column.	 */
static MARKRING mouse_saved_ring; /* Mark ring before the drag.   */

/*
 * Click counting.  The SGR protocol reports raw presses and releases,
 * so consecutive plain clicks on one cell are counted here: the
 * second selects the word at the clicked cell, the third the whole
 * line.  The count breaks when a click lands elsewhere or any other
 * mouse event intervenes.
 */
static int mouse_click_streak;	/* Consecutive plain clicks.	 */
static int mouse_click_row;	/* Cell of the last click, row.	 */
static int mouse_click_col;	/* Cell of the last click, col.	 */

static void
mouse_click_reset (void)
{
  mouse_click_streak = 0;
}

/*
 * Move the point to the cell under the mouse in the text area of
 * the window below.  When "only" is not NULL the point moves only
 * within that window, so a drag stays in the window it started in.
 * Returns the window whose point moved, or NULL when the mouse is
 * not over text (below the last window, on a mode line, or outside
 * "only").
 */
static EWINDOW *
mouse_drop_dot (const EMOUSE *event, EWINDOW *only)
{
  EWINDOW *wp;
  LINE *line;
  int row;
  int text_column;

  wp = mouse_find_window (event->row);
  if (wp == NULL || (only != NULL && wp != only))
    return NULL;
  if (event->row == wp->w_toprow + wp->w_ntrows)
    return NULL;			/* Mode line, not text.  */

  line = wp->w_linep;
  for (row = wp->w_toprow; row < event->row; ++row)
    {
      if (line == wp->w_bufp->b_linep)
        break;
      line = lforw (line);
    }
  if (line == wp->w_bufp->b_linep && lback (line) != line)
    line = lback (line);

  mouse_switch_window (wp);

  text_column = event->column - window_gutter_width (wp)
                + wp->w_leftcol;
  wp->w_dot.p = line;
  wp->w_dot.o = mouse_line_offset (line, text_column);
  wp->w_hscroll = FALSE;
  wp->w_flag |= WFMOVE;
  return wp;
}

/*
 * Scroll for a wheel event: vertically by mouse-wheel-step lines in
 * the window under the mouse, button 3 up and button 4 down.  A
 * shifted wheel scrolls horizontally instead, by the scroll-left and
 * scroll-right columns, and the window is pinned (w_hscroll) so
 * update()'s automatic recentring around dot does not pull the view
 * straight back.  A wheel report outside every window scrolls the
 * current window.
 */
static int
mouse_wheel_scroll (const EMOUSE *event)
{
  EWINDOW *wp;

  wp = mouse_find_window (event->row);
  if (wp == NULL)
    wp = curwp;
  mouse_switch_window (wp);
  if ((event->modifiers & MOUSE_SHIFT) != 0)
    {
      if (event->button == 3)
        return scrollleft (TRUE, 4, KRANDOM);
      return scrollright (TRUE, 4, KRANDOM);
    }
  return mvupwind (FALSE,
                   event->button == 3 ? mouse_wheel_step
                                      : -mouse_wheel_step,
                   KRANDOM);
}

/*
 * Select the word around the point of window "wp": the run of word
 * characters, or the run of blanks, that the point sits in.  The
 * selection stays on one line.  Returns FALSE when the point is at
 * the end of the line and there is nothing to select.
 */
static int
mouse_select_word (EWINDOW *wp)
{
  LINE *lp;
  int len;
  int o;
  int word;

  lp = wp->w_dot.p;
  len = wllength (lp);
  o = wp->w_dot.o;
  if (o >= len)
    return FALSE;
  word = CISWORD (wlgetc (lp, o)) != 0;
  while (o > 0 && (CISWORD (wlgetc (lp, o - 1)) != 0) == word)
    --o;
  wp->w_mark.p = lp;
  wp->w_mark.o = o;
  o = wp->w_dot.o;
  while (o < len && (CISWORD (wlgetc (lp, o)) != 0) == word)
    ++o;
  wp->w_dot.o = o;
  return TRUE;
}

/*
 * Select the line the point of window "wp" is on, from its first
 * to its last character.
 */
static void
mouse_select_line (EWINDOW *wp)
{
  wp->w_mark.p = wp->w_dot.p;
  wp->w_mark.o = 0;
  wp->w_dot.o = wllength (wp->w_dot.p);
}

/*
 * A middle event yanks the kill buffer at the clicked cell; a right
 * event extends the region to the clicked cell.  Returns TRUE when
 * the button was one of these, FALSE for the left button.
 */
static int
mouse_button_click (const EMOUSE *event)
{
  EWINDOW *wp;

  if (event->button == 1)
    {
      endmouseselect ();
      if (mouse_drop_dot (event, NULL) != NULL)
        (void) yank (FALSE, 1, KRANDOM);
      return TRUE;
    }
  if (event->button == 2)
    {
      wp = mouse_drop_dot (event, NULL);
      if (wp != NULL && wp->w_mark.p != NULL)
        {
          mouseselect = TRUE;
          wp->w_flag |= WFHARD;
        }
      else
        endmouseselect ();
      return TRUE;
    }
  return FALSE;
}

/*
 * Handle a mouse event.  The tty backend decodes the report into an
 * EMOUSE event on the queue.
 *
 * A wheel event scrolls the window under the mouse by
 * mouse-wheel-step lines, or horizontally when the wheel is shifted.
 * A press in the text of a window moves
 * the point there and anchors a drag: the mark ring before the
 * drag is remembered and the mark is dropped on the pressed cell
 * right away, because terminals differ in whether they report
 * motion while the button is held.  Motion during the drag extends
 * the region, and the release position completes it even when no
 * motion was reported; a press and release on the same cell is a
 * plain click, and the saved mark ring goes back, so clicking does
 * not disturb an existing mark.  Consecutive plain clicks on one
 * cell select the word (second click) or the whole line (third
 * click) at the clicked cell.  A middle event yanks the kill
 * buffer at the clicked cell, and a right event extends the region
 * to the clicked cell.  A press or click on a mode line selects
 * that window.  A click that the tty layer reports as one complete
 * MOUSE_CLICK gesture is handled the same way, with the click
 * count supplied by the backend.
 */
int
mouseevent (int f, int n, int k)
{
  EMOUSE event;
  EWINDOW *wp;

  if (mousedrop (&event) == FALSE)
    return FALSE;

  if (event.kind == MOUSE_WHEEL)
    {
      mouse_click_reset ();
      return mouse_wheel_scroll (&event);
    }

  /* A click reported as one event is a whole press/release gesture
     with the backend's click count. */
  if (event.kind == MOUSE_CLICK)
    {
      mouse_dragging = FALSE;
      mouse_click_reset ();
      wp = mouse_find_window (event.row);
      if (wp == NULL)
        return (TRUE);
      if (event.row == wp->w_toprow + wp->w_ntrows)
        {
          mouse_switch_window (wp);
          endmouseselect ();
          return (TRUE);
        }
      if (mouse_button_click (&event) != FALSE)
        return (TRUE);
      if (event.clicks >= 3)
        {
          wp = mouse_drop_dot (&event, NULL);
          if (wp != NULL)
            {
              mouse_select_line (wp);
              mouseselect = TRUE;
              wp->w_flag |= WFHARD;
            }
          return (TRUE);
        }
      if (event.clicks == 2)
        {
          wp = mouse_drop_dot (&event, NULL);
          if (wp != NULL && mouse_select_word (wp) != FALSE)
            {
              mouseselect = TRUE;
              wp->w_flag |= WFHARD;
            }
          else
            endmouseselect ();
          return (TRUE);
        }
      endmouseselect ();
      (void) mouse_drop_dot (&event, NULL);
      return (TRUE);
    }

  /* A release ends the drag.  A release away from the pressed cell
     completes the region at the release position even on terminals
     that report no motion while the button is held; a release on
     the pressed cell without motion was a plain click, so the
     saved mark ring goes back. */
  if (event.kind == MOUSE_RELEASE)
    {
      int keep;			/* Region survives the release.  */

      if (mouse_dragging && event.button == 0)
        {
          mouse_dragging = FALSE;
          keep = FALSE;
          if (mouse_drag_moved
              || event.row != mouse_anchor_row
              || event.column != mouse_anchor_col)
            {
              /* A release outside the window the drag started in
                 keeps whatever motion already selected. */
              keep = mouse_drop_dot (&event, mouse_anchor_wp) != NULL
                     || mouse_drag_moved;
            }
          if (!keep)
            {
              /* Plain click after all: the saved mark ring goes
                 back.  Consecutive plain clicks on one cell select
                 the word, then the whole line. */
              mouse_click_streak
                = (event.row == mouse_click_row
                   && event.column == mouse_click_col)
                  ? mouse_click_streak + 1 : 1;
              mouse_click_row = event.row;
              mouse_click_col = event.column;
              mouse_anchor_wp->w_ring = mouse_saved_ring;
              if (mouse_click_streak == 2
                  && mouse_select_word (mouse_anchor_wp) != FALSE)
                {
                  mouseselect = TRUE;
                  mouse_anchor_wp->w_flag |= WFHARD;
                }
              else if (mouse_click_streak >= 3)
                {
                  mouse_select_line (mouse_anchor_wp);
                  mouseselect = TRUE;
                  mouse_anchor_wp->w_flag |= WFHARD;
                }
              else
                {
                  endmouseselect ();
                  mouse_anchor_wp->w_flag |= WFMOVE;
                }
            }
          else
            {
              mouse_click_reset ();
              mouse_anchor_wp->w_flag |= WFHARD;
            }
        }
      return (TRUE);
    }

  /* Idle motion without a drag is ignored. */
  if (event.kind == MOUSE_MOTION && !mouse_dragging)
    {
      mouse_click_reset ();
      return (TRUE);
    }

  /* A fresh press anchors a drag on the pressed cell; a middle or
     right press acts like the matching click, and a press on a
     mode line selects that window. */
  if (!mouse_dragging)
    {
      if (event.kind == MOUSE_PRESS && event.modifiers == 0)
        {
          wp = mouse_find_window (event.row);
          if (wp == NULL)
            {
              endmouseselect ();
              mouse_click_reset ();
              return (TRUE);
            }
          if (event.row == wp->w_toprow + wp->w_ntrows)
            {
              mouse_switch_window (wp);
              endmouseselect ();
              mouse_click_reset ();
              return (TRUE);
            }
          if (event.button != 0)
            {
              mouse_click_reset ();
              return mouse_button_click (&event);
            }
          wp = mouse_drop_dot (&event, NULL);
          if (wp == NULL)
            return (TRUE);
          mouse_dragging = TRUE;
          mouse_drag_moved = FALSE;
          mouse_anchor_wp = wp;
          mouse_anchor_row = event.row;
          mouse_anchor_col = event.column;
          mouse_saved_ring = wp->w_ring;
          wp->w_mark = wp->w_dot;
          mouseselect = TRUE;
          wp->w_flag |= WFMOVE | WFHARD;
          return (TRUE);
        }

      mouse_click_reset ();
      eprintf ("[Mouse kind %d, button %d, row %d, column %d, "
               "modifiers %d, clicks %d]",
               event.kind, event.button, event.row, event.column,
               event.modifiers, event.clicks);
      return (TRUE);
    }

  /* Dragging: motion reports extend the region by moving the point
     within the window the drag started in. */
  mouse_click_reset ();
  if (mouse_drop_dot (&event, mouse_anchor_wp) != NULL)
    {
      mouse_drag_moved = TRUE;
      mouse_anchor_wp->w_flag |= WFHARD;
    }
  return (TRUE);
}

/*
 * Turn off the selection highlight.  Every window whose mark is
 * set may be showing it, and needs a full repaint.
 */
void
endmouseselect (void)
{
  EWINDOW *wp;

  if (mouseselect == FALSE)
    return;
  mouseselect = FALSE;
  ALLWIND (wp)
    if (wp->w_mark.p != NULL)
      wp->w_flag |= WFHARD;
}

int
togglemouse (int f, int n, int k)
{
  (void) f;
  (void) n;
  (void) k;

  mouse = !mouse;
  mousereset ();
  ttsetmouse (mouse);
  eprintf ("[Mouse now %s]", mouse ? "ON" : "OFF");
  return TRUE;
}

#endif
