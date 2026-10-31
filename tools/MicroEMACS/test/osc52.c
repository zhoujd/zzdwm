#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _XOPEN_SOURCE
#include <wchar.h>

#include "../osc52.c"

static char last_osc52[512];
static int osc52_calls;

void
ttosc52 (const char *encoded)
{
  ++osc52_calls;
  snprintf (last_osc52, sizeof (last_osc52), "%s", encoded);
}

void
eprintf (const char *format, ...)
{
  (void) format;
}

int
ereply (const char *prompt, char *buffer, int size, ...)
{
  (void) prompt;
  (void) buffer;
  (void) size;
  return FALSE;
}

int
linsert (int count, int character, char *buffer)
{
  (void) count;
  (void) character;
  (void) buffer;
  return TRUE;
}

static LINE *
make_line (const char *text)
{
  size_t length = strlen (text);
  LINE *line = calloc (1, LINEHDR_SIZE + length + 1);

  assert (line != NULL);
  line->l_size = length + 1;
  line->l_used = length;
  memcpy (line->l_text, text, length);
  return line;
}

static void
send_region (LINE *line, int offset, int size)
{
  REGION region;

  region.r_pos.p = line;
  region.r_pos.o = offset;
  region.r_size = size;
  osc52send (&region);
}

int
main (void)
{
  LINE *first;
  LINE *second;
  LINE *third;
  LINE *end;
  int calls;

  first = make_line ("alpha beta");
  second = make_line ("gamma delta");
  third = make_line ("epsilon");
  end = make_line ("");
  first->l_fp = second;
  second->l_bp = first;
  second->l_fp = third;
  third->l_bp = second;
  third->l_fp = end;
  end->l_bp = third;
  end->l_fp = first;
  first->l_bp = end;

  assert (osc52 == TRUE);
  osc52_calls = 0;

  send_region (first, 6, 4 + 1 + 11 + 1 + 3);
  assert (osc52_calls == 1);
  assert (strcmp (last_osc52, "YmV0YQpnYW1tYSBkZWx0YQplcHM=") == 0);

  send_region (first, 0, 10 + 1 + 11);
  assert (strcmp (last_osc52, "YWxwaGEgYmV0YQpnYW1tYSBkZWx0YQ==") == 0);

  send_region (second, 0, 5);
  assert (strcmp (last_osc52, "Z2FtbWE=") == 0);

  send_region (first, 6, 4);
  assert (strcmp (last_osc52, "YmV0YQ==") == 0);

  free (third);
  free (end);
  third = make_line ("aé中");
  end = make_line ("");
  third->l_fp = end;
  end->l_bp = third;
  end->l_fp = first;
  first->l_bp = end;
  send_region (third, 0, 3);
  assert (strcmp (last_osc52, "YcOp5Lit") == 0);

  calls = osc52_calls;
  osc52 = FALSE;
  send_region (second, 0, 5);
  assert (osc52_calls == calls);
  osc52 = TRUE;

  free (first);
  free (second);
  free (third);
  free (end);
  puts ("osc52 tests passed");
  return 0;
}
