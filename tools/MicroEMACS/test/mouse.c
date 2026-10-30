/*
 * Tests for mouse click column mapping (mouse_col_to_offset in
 * display.c), included directly so the shared display code is
 * exercised with the same configuration as display-test.
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

int
main (void)
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

  puts ("mouse tests passed");
  return 0;
}
