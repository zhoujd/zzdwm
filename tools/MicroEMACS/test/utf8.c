#include <stdio.h>
#include <string.h>
#include <time.h>

#include "def.h"

int
main (void)
{
  static uchar s[] = {
    'a', '=', 0xc3, 0xa4, ',', 'i', '=', 0xe2, 0x88, 0xab, ',',
    '+', '=', 0xf0, 0x90, 0x80, 0x8f, ',', 'j', '=', 0xf0, 0x9f,
    0x82, 0xab, '.', 'a', '=', 0xc3, 0xa4, ',', 'i', '=',
    0xe2, 0x88, 0xab, ',', '+', '=', 0xf0, 0x90, 0x80, 0x8f, ',',
    'j', '=', 0xf0, 0x9f, 0x82, 0xab, '.', 0
  };
  wchar_t c;
  const uchar *p;
  double time_spent;
  clock_t begin;
  clock_t end;
  int i;
  int len;

  printf ("size of wchar_t is %ld\n", sizeof (c));
  printf ("s is '%s'\n", s);
  printf ("length of s is %ld\n", sizeof (s));
  len = uslen (s);
  printf ("number of UTF-8 chars in s is %d\n", len);
  printf ("Scanning forward...\n");
  for (i = 0; i < len; ++i)
    {
      int size;
      int u = ugetc (s, i, &size);

      printf ("offset of UTF-8 char #%d in s is %ld\n", i,
              ugetcptr (s, i) - s);
      printf ("char #%d in s in unicode is %x, size %d\n", i, u, size);
    }
  printf ("Scanning backwards...\n");
  p = s + strlen ((const char *) s);
  i = len;
  while (p > s)
    {
      int size;
      int u = ugetprevc (p, &size);

      p -= size;
      --i;
      printf ("offset of UTF-8 char #%d in s is %ld\n", i, p - s);
      printf ("char #%d in s in unicode is %x, size %d\n", i, u, size);
    }
  begin = clock ();
  for (i = 0; i < 10000000; ++i)
    if ((p = ugetcptr (s, len)) == s)
      printf ("Should never get here!\n");
  end = clock ();
  time_spent = (double) (end - begin) / CLOCKS_PER_SEC;
  printf ("Time spent in %d loops of ugetcptr is %f\n", i, time_spent);
  return 0;
}
