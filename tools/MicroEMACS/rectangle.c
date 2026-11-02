/*
 * Filename rectangle.c
 */

#include "def.h"
#include <stdio.h>

/*
 * =========================================================================
 * RECTANGLE ENGINE CONFIGURATION & STRUCTURES (Add to def.h if needed)
 * =========================================================================
 */
#define MAX_RECT_LINES 512

/* Structure to act as our dedicated column-based clipboard */
typedef struct RECT_CLIPBOARD
{
  char *lines[MAX_RECT_LINES];
  int line_count;
  int width;
}
RECT_CLIPBOARD;

static RECT_CLIPBOARD rect_kill_buffer = { { NULL }, 0, 0 };

/* Global tracking states (Ensure these are defined in def.h / main.c) */
int rect_start_line = 0;
int rect_start_col = 0;
int rect_active = FALSE;

/*
 * Helper: Calculates the normalized bounding box.
 * This handles rectangles drawn backwards (bottom-to-top or right-to-left).
 */
static void
get_rect_bounds (int *s_row, int *e_row, int *s_col, int *e_col)
{
  /*
   * TODO: MicroEmacs tracks lines via a linked list of LINE structures
   * (w_dotp). You will need to calculate the current integer row number of
   * curwp->w_dotp relative to the top of the buffer to compare it against
   * rect_start_line.
   */
  int current_row = 1; /* Placeholder: Implement get_current_row_index() */
  int current_col
      = getccol (FALSE); /* Native MicroEmacs function for current column */

  *s_row = (rect_start_line < current_row) ? rect_start_line : current_row;
  *e_row = (rect_start_line > current_row) ? rect_start_line : current_row;
  *s_col = (rect_start_col < current_col) ? rect_start_col : current_col;
  *e_col = (rect_start_col > current_col) ? rect_start_col : current_col;
}

/*
 * Helper: Ensures a line structure physically has enough characters to reach
 * target_col. If it's too short, it pads it out with native space characters.
 */
static int
pad_line_to_col (LINE *lp, int target_col)
{
  while (lp->l_used < target_col)
    {
      /*
       * TODO: Call your native character insertion primitive.
       * Traditional MicroEmacs uses: linsert(1, ' ');
       * This expands lp->l_text and increments lp->l_used.
       */
      if (linsert (1, ' ') == FALSE)
        return FALSE;
    }
  return TRUE;
}

/*
 * =========================================================================
 * CORE RECTANGLE INTERACTIVE COMMANDS
 * =========================================================================
 */

/* Toggle Rectangle Mode on/off and drop anchor */
int
toggle_rectangle_mode (int f, int n, int k)
{
  if (rect_active)
    {
      rect_active = FALSE;
      eprintf ("[Rectangle Mode Disabled]");
    }
  else
    {
      rect_active = TRUE;
      /* Record anchoring position */
      rect_start_col = getccol (FALSE);
      rect_start_line = 1; /* TODO: Assign actual current row index */
      eprintf ("[Rectangle Mode Enabled]");
    }
  curwp->w_flag |= WFMOVE; /* Force MicroEmacs window redraw flag */
  return TRUE;
}

/* Copy text within the bounds into the dedicated column clipboard */
int
copy_rectangle (int f, int n)
{
  int s_row, e_row, s_col, e_col;
  get_rect_bounds (&s_row, &e_row, &s_col, &e_col);

  /* TODO: Free old strings inside rect_kill_buffer to avoid memory leaks */
  rect_kill_buffer.line_count = 0;
  rect_kill_buffer.width = e_col - s_col;

  /* Find starting line structure pointer */
  LINE *lp = curbp->b_linep->l_forw; /* Start at buffer top line */
  // TODO: Loop lp = lforw(lp) until reaching s_row

  for (int r = s_row; r <= e_row; r++)
    {
      int width = e_col - s_col;
      /* Allocate buffer slice */
      char *slice = (char *)malloc (width + 1);

      /* Copy out characters safely balancing line lengths */
      for (int c = 0; c < width; c++)
        {
          int actual_col = s_col + c;
          if (actual_col < lp->l_used)
            {
              slice[c] = lp->l_text[actual_col];
            }
          else
            {
              slice[c] = ' '; /* Virtual white space padding */
            }
        }
      slice[width] = '\0';
      rect_kill_buffer.lines[rect_kill_buffer.line_count++] = slice;

      lp = lforw (lp);
    }

  rect_active = FALSE;
  eprintf ("[Rectangle Copied]");
  return TRUE;
}

/* Delete characters within column bounds, shifting right side leftwards */
int
kill_rectangle (int f, int n, int k)
{
  /* First, copy it so it behaves like a true cut operation */
  copy_rectangle (FALSE, 1);

  int s_row, e_row, s_col, e_col;
  get_rect_bounds (&s_row, &e_row, &s_col, &e_col);
  int width = e_col - s_col;

  LINE *lp = curbp->b_linep->l_forw;
  // TODO: Navigate lp pointer to s_row

  for (int r = s_row; r <= e_row; r++)
    {
      if (lp->l_used > s_col)
        {
          /* Position cursor line internally to process modifications */
          curwp->w_dotp = lp;
          curwp->w_doto = s_col;

          /* Calculate true width available to delete on this line */
          int to_delete
              = (s_col + width > lp->l_used) ? (lp->l_used - s_col) : width;

          /* Native MicroEmacs deletion loop or internal ldelete call */
          ldelete ((long)to_delete, FALSE);
        }
      lp = lforw (lp);
    }

  sgarbf = TRUE; /* Force a complete terminal frame refresh */
  return TRUE;
}

/* Insert clipboard column content down row intersections */
int
yank_rectangle (int f, int n, int k)
{
  if (rect_kill_buffer.line_count == 0)
    {
      eprintf ("Error: Rectangle clipboard is empty.");
      return FALSE;
    }

  LINE *lp = curwp->w_dotp;
  int target_col = curwp->w_doto;

  for (int i = 0; i < rect_kill_buffer.line_count; i++)
    {
      if (lp == curbp->b_linep)
        break; /* Avoid passing buffer boundary */

      curwp->w_dotp = lp;
      pad_line_to_col (lp, target_col);
      curwp->w_doto = target_col;

      /* Insert text string at this line's target column */
      char *str = rect_kill_buffer.lines[i];
      while (*str)
        {
          linsert (1, *str++);
        }

      lp = lforw (lp);
    }

  sgarbf = TRUE;
  return TRUE;
}

/* Blank out structural components inside boundaries without changing text
 * positions */
int
clear_rectangle (int f, int n, int k)
{
  int s_row, e_row, s_col, e_col;
  get_rect_bounds (&s_row, &e_row, &s_col, &e_col);

  LINE *lp = curbp->b_linep->l_forw;
  // TODO: Navigate lp pointer to s_row

  for (int r = s_row; r <= e_row; r++)
    {
      for (int c = s_col; c < e_col; c++)
        {
          if (c < lp->l_used)
            {
              lp->l_text[c] = ' '; /* Overwrite with spaces */
            }
        }
      lp = lforw (lp);
    }

  lchange (WFMOD); /* Mark buffer altered/dirty state */
  return TRUE;
}

/* Shift text columns eastwardly by inserting blank columns */
int
open_rectangle (int f, int n, int k)
{
  int s_row, e_row, s_col, e_col;
  get_rect_bounds (&s_row, &e_row, &s_col, &e_col);
  int width = e_col - s_col;

  LINE *lp = curbp->b_linep->l_forw;
  // TODO: Navigate lp pointer to s_row

  for (int r = s_row; r <= e_row; r++)
    {
      curwp->w_dotp = lp;
      pad_line_to_col (lp, s_col);
      curwp->w_doto = s_col;

      for (int i = 0; i < width; i++)
        {
          linsert (1, ' ');
        }
      lp = lforw (lp);
    }

  sgarbf = TRUE;
  return TRUE;
}
