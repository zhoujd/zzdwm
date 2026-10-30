#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../mouse.c"

static void
expect_event (MOUSE_EVENT event, int kind, int button, int row, int column,
              int modifiers)
{
  assert (event.kind == kind);
  assert (event.button == button);
  assert (event.row == row);
  assert (event.column == column);
  assert (event.modifiers == modifiers);
  assert (event.clicks == 0);
}

int
main (void)
{
  MOUSE_EVENT event;
  int index;

  mousereset ();
  assert (mouseeventcount () == 0);
  assert (mousepeek (&event) == FALSE);
  assert (mousedrop (&event) == FALSE);
  assert (mouseenqueue (NULL) == FALSE);

  memset (&event, 0, sizeof (event));
  event.kind = MOUSE_PRESS;
  event.button = 0;
  event.row = 3;
  event.column = 7;
  for (index = 0; index < MOUSE_QUEUE_SIZE; ++index)
    assert (mouseenqueue (&event) == TRUE);
  assert (mouseeventcount () == MOUSE_QUEUE_SIZE);
  assert (mouseenqueue (&event) == FALSE);
  assert (mousedrop (&event) == TRUE && event.column == 7);
  assert (mouseeventcount () == MOUSE_QUEUE_SIZE - 1);
  memset (&event, 0, sizeof (event));
  event.kind = MOUSE_PRESS;
  event.button = 0;
  event.row = 4;
  event.column = 7;
  assert (mouseenqueue (&event) == TRUE);
  assert (mousedrop (&event) == TRUE && event.row == 3);
  for (index = 0; index < MOUSE_QUEUE_SIZE - 2; ++index)
    assert (mousedrop (&event) == TRUE);
  assert (mouseeventcount () == 1);
  assert (mousepeek (&event) == TRUE && event.row == 4);
  assert (mousedrop (&event) == TRUE && event.row == 4);
  assert (mouseeventcount () == 0);

  mousereset ();
  assert (mousedecodesgr (&event, -1, 0, 0, FALSE) == FALSE);
  assert (mousedecodesgr (&event, 0, -1, 0, FALSE) == FALSE);
  assert (mousedecodesgr (&event, 0, 0, -1, FALSE) == FALSE);
  assert (mousedecodesgr (NULL, 0, 0, 0, FALSE) == FALSE);
  assert (mousedecodesgr (&event, 0, 2, 5, FALSE) == TRUE);
  expect_event (event, MOUSE_PRESS, 0, 2, 5, 0);
  assert (mousedecodesgr (&event, 0, 2, 5, TRUE) == TRUE);
  expect_event (event, MOUSE_RELEASE, 0, 2, 5, 0);
  assert (mousedecodesgr (&event, 4 | 8 | 16, 2, 5, FALSE) == TRUE);
  expect_event (event, MOUSE_PRESS, 0, 2, 5,
                MOUSE_SHIFT | MOUSE_ALT | MOUSE_CTRL);
  assert (mousedecodesgr (&event, 32, 2, 5, FALSE) == TRUE);
  expect_event (event, MOUSE_MOTION, 0, 2, 5, 0);
  assert (mousedecodesgr (&event, 64, 2, 5, FALSE) == TRUE);
  expect_event (event, MOUSE_WHEEL, 3, 2, 5, 0);
  assert (mousedecodesgr (&event, 65, 2, 5, FALSE) == TRUE);
  expect_event (event, MOUSE_WHEEL, 4, 2, 5, 0);

  puts ("mouse tests passed");
  return 0;
}
