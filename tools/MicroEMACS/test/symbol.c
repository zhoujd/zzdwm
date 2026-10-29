#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#include "../symbol.c"

static char error_message[128];
static int next_getkey;
static int abort_calls;
static int regular_otherwind_calls;
static int regexp_otherwind_calls;
static int macro_keys[1];

int kbdm[NKBDM] = { KCTLX | ')' };
int *kbdmip;
int *kbdmop;

int
ctrlg (int f, int n, int k)
{
  (void) f;
  (void) n;
  ++abort_calls;
  if (kbdmip != NULL)
    {
      kbdm[0] = (KCTLX | ')');
      kbdmip = NULL;
    }
  return ABORT;
}

void
ttbeep (void)
{
}

int
getkey (void)
{
  int key;

  assert (next_getkey != 0);
  key = next_getkey;
  next_getkey = 0;
  return key;
}

void
eprintf (const char *format, ...)
{
  va_list args;

  va_start (args, format);
  vsnprintf (error_message, sizeof (error_message), format, args);
  va_end (args);
}

int
findtagotherwind (int f, int n, int k)
{
  (void) f;
  (void) n;
  (void) k;
  ++regular_otherwind_calls;
  return TRUE;
}

int
findtagregotherwind (int f, int n, int k)
{
  (void) f;
  (void) n;
  (void) k;
  ++regexp_otherwind_calls;
  return TRUE;
}

int
main (void)
{
  next_getkey = KCTRL | 'G';
  kbdmip = kbdm;
  assert (otherwindprefix (TRUE, 1, 0) == ABORT);
  assert (abort_calls == 1);
  assert (kbdmip == NULL);
  assert (kbdm[0] == (KCTLX | ')'));
  assert (strcmp (error_message, "[Aborted]") == 0);

  macro_keys[0] = KCTRL | 'G';
  kbdmop = macro_keys;
  assert (otherwindprefix (TRUE, 1, 0) == ABORT);
  assert (abort_calls == 2);
  assert (kbdmop == &macro_keys[1]);
  kbdmop = NULL;

  next_getkey = '.';
  assert (otherwindprefix (TRUE, 1, 0) == TRUE);
  assert (regular_otherwind_calls == 1);

  next_getkey = KMETA | KCTRL | '.';
  assert (otherwindprefix (TRUE, 1, 0) == TRUE);
  assert (regexp_otherwind_calls == 1);

  next_getkey = 'X';
  assert (otherwindprefix (TRUE, 1, 0) == ABORT);
  assert (strcmp (error_message, "Unknown C-X 4 command") == 0);

  printf ("symbol tests passed\n");
  return 0;
}
