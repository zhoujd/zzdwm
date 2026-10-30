/*
 * Filename mouse.c
 */

#include "def.h"

static MOUSE_EVENT mouse_queue[MOUSE_QUEUE_SIZE];
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
mouseenqueue (const MOUSE_EVENT *event)
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
mousepeek (MOUSE_EVENT *event)
{
  if (event == NULL || mouse_queue_count == 0)
    return FALSE;

  *event = mouse_queue[mouse_queue_head];
  return TRUE;
}

int
mousedrop (MOUSE_EVENT *event)
{
  if (mousepeek (event) == FALSE)
    return FALSE;

  mouse_queue_head = (mouse_queue_head + 1) % MOUSE_QUEUE_SIZE;
  --mouse_queue_count;
  return TRUE;
}

int
mousedecodesgr (MOUSE_EVENT *event, int code, int row, int column,
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
