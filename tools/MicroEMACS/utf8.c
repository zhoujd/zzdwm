/*
    Copyright (C) 2018 Mark Alexander

    This file is part of MicroEMACS, a small text editor.

    MicroEMACS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * The standalone test driver for this file is in test/utf8.c.
 */

#define _XOPEN_SOURCE
#include <wchar.h>

#include "def.h"

#include <string.h>
#include <stdio.h>

/*
 * Return true if Unicode character c is a combining character,
 * i.e. is a non-spacing character that combines
 * with a subsequent one.
 */
int
ucombining (wchar_t c)
{
  return ((c >= 0x300  && c <= 0x36f)  ||
          (c >= 0x1ab0 && c <= 0x1aff) ||
          (c >= 0x1dc0 && c <= 0x1dff) ||
          (c >= 0x20d0 && c <= 0x20ff) ||
          (c >= 0xfe20 && c <= 0xfe2f));
}

/*
 * Return the address of the nth UTF-8 character in the string s.
 */
const uchar *
ugetcptr (const uchar *s, int n)
{
  while (n > 0)
    {
      s += uclen (s);
      --n;
    }
  return s;
}

/*
 * Return the byte offset of the nth UTF-8 character in the string s.
 */
int
uoffset (const uchar *s, int n)
{
  const uchar *start = s;
  while (n > 0)
    {
       s += uclen (s);
      --n;
    }
  return s - start;
}

/*
 * Return number of UTF-8 characters in the null-terminated string s.
 */
int
uslen (const uchar *s)
{
  int len = 0;

  while (*s != 0)
    {
      s += uclen (s);
      len++;
    }
  return len;
}

/*
 * Return number of UTF-8 characters in the string s of length n.
 */
int
unslen (const uchar *s, int n)
{
  int len = 0;
  const uchar *end = s + n;

  while (s < end)
    {
      s += uclen (s);
      len++;
    }
  return len;
}

/*
 * Return the number of bytes used by the next n UFT-8 characters
 * in the string s.
 */
int
unblen (const uchar *s, int n)
{
  const uchar *start = s;

  while (n > 0)
    {
      s += uclen (s);
      --n;
    }
  return s - start;
}

/*
 * Get the nth UTF-8 character in s, return it
 * as a 32-bit unicode character.  If len is not NULL,
 * return the length of the UTF-8 character to *len.
 */
wchar_t
ugetc (const uchar *s, int n, int *len)
{
  uchar c;

  s = ugetcptr (s, n);
  c = *s;
  if (c < 0x80)
    {
      if (len)
        *len = 1;
      return c;
    }
  if (c >= 0xc0 && c <= 0xdf)
    {
      if (len)
        *len = 2;
      return ((int)(c & 0x1f) << 6) + (int)(s[1] & 0x3f);
    }
  if (c >= 0xe0 && c <= 0xef)
    {
      if (len)
        *len = 3;
      return ((int)(c & 0xf) << 12) +
             ((int)(s[1] & 0x3f) << 6) +
             (int)(s[2] & 0x3f);
    }
  if (c >= 0xf0 && c <= 0xf7)
    {
      if (len)
        *len = 4;
      return ((int)(c & 0x7) << 18) +
             ((int)(s[1] & 0x3f) << 12) +
             ((int)(s[2] & 0x3f) << 6) +
             (int)(s[3] & 0x3f);
    }
  if (c >= 0xf8 && c <= 0xfb)
    {
      if (len)
        *len = 5;
      return ((int)(c & 0x3) << 24) +
             ((int)(s[1] & 0x3f) << 18) +
             ((int)(s[2] & 0x3f) << 12) +
             ((int)(s[3] & 0x3f) << 6) +
             (int)(s[4] & 0x3f);
    }
  if (c >= 0xfc && c <= 0xfd)
    {
      if (len)
        *len = 6;
      return ((int)(c & 0x1) << 30) +
             ((int)(s[1] & 0x3f) << 24) +
             ((int)(s[2] & 0x3f) << 18) +
             ((int)(s[3] & 0x3f) << 12) +
             ((int)(s[4] & 0x3f) << 6) +
             (int)(s[5] & 0x3f);
    }
  /* Error */
  if (len)
    *len = 1;
  return c;
}
/*
 * Get the previous UTF-8 character, i.e. the character just
 * before the one pointed to by s.  Return it
 * as a 32-bit unicode character.  If len is not NULL,
 * return the length of the UTF-8 character to *len.
 */
wchar_t
ugetprevc (const uchar *s, int *len)
{
  do {
    --s;
  } while ((*s & 0xc0) == 0x80);
  return ugetc (s, 0, len);
}

/*
 * Convert a Unicode character c to UTF-8, writing the
 * characters to s; s must be at least 6 bytes long.
 * Return the number of bytes in the UTF-8 string.
 */
int
uputc (wchar_t ch, uchar *s)
{
  unsigned int c = (unsigned int)ch;
  if (c < 0x80)
    {
      s[0] = c;
      return 1;
    }
  if (c >= 0x80 && c <= 0x7ff)
    {
      s[0] = 0xc0 | ((c >> 6) & 0x1f);
      s[1] = 0x80 | (c & 0x3f);
      return 2;
    }
  if (c >= 0x800 && c <= 0xffff)
    {
      s[0] = 0xe0 | ((c >> 12) & 0x0f);
      s[1] = 0x80 | ((c >>  6) & 0x3f);
      s[2] = 0x80 | (c & 0x3f);
      return 3;
    }
  if (c >= 0x10000 && c <= 0x1fffff)
    {
      s[0] = 0xf0 | ((c >> 18) & 0x07);
      s[1] = 0x80 | ((c >> 12) & 0x3f);
      s[2] = 0x80 | ((c >>  6) & 0x3f);
      s[3] = 0x80 | (c & 0x3f);
      return 4;
    }
  if (c >= 0x200000 && c <= 0x3ffffff)
    {
      s[0] = 0xf8 | ((c >> 24) & 0x03);
      s[1] = 0x80 | ((c >> 18) & 0x3f);
      s[2] = 0x80 | ((c >> 12) & 0x3f);
      s[3] = 0x80 | ((c >>  6) & 0x3f);
      s[4] = 0x80 | (c & 0x3f);
      return 5;
    }
  if (c >= 0x4000000 && c <= 0x7fffffff)
    {
      s[0] = 0xfc | ((c >> 30) & 0x01);
      s[1] = 0x80 | ((c >> 24) & 0x3f);
      s[2] = 0x80 | ((c >> 18) & 0x3f);
      s[3] = 0x80 | ((c >> 12) & 0x3f);
      s[4] = 0x80 | ((c >>  6) & 0x3f);
      s[5] = 0x80 | (c & 0x3f);
      return 6;
    }
  /* Error */
  s[0] = c;
  return 1;
}

/*
 * Return the display width of a Unicode character.
 * This is just a wrapper for wcwidth.
 */
int
uwidth (wchar_t ch)
{
#if defined(MINGW) || defined(_WIN32)
  unsigned int c = (unsigned int)ch;
  if (c == 0) return 0;
  if (c < 32 || (c >= 0x7f && c < 0xa0)) return -1;

  /* Zero-Width Marks, Invisible Operators, Diacritics & Variation Selectors */
  if ((c >= 0x0300 && c <= 0x036f) || /* Combining Diacritical Marks */
      (c >= 0x0e31 && c <= 0x0e3a) || /* Thai Vowels & Combining Marks */
      (c >= 0x0e47 && c <= 0x0e4e) || /* Thai Tone Marks */
      (c >= 0x200b && c <= 0x200d) || /* Zero-Width Space, Non-Joiner, Joiner */
      (c >= 0x2060 && c <= 0x206f) || /* Invisible Operators (Invisible Plus U+2064, etc.) */
      (c >= 0x20d0 && c <= 0x20ff) || /* Combining Marks for Symbols */
      (c >= 0xfe00 && c <= 0xfe0f))   /* Variation Selectors (e.g., Emoji Variation Selector-16) */
    {
      return 0;
    }

  /* Math Delimiters, Operators & Box Art (Width = 1) */
  if ((c >= 0x2200 && c <= 0x22ff) || /* Math Operators (∮, ∑, ∏, ∀, ∈) */
      (c >= 0x2300 && c <= 0x23ff) || /* Technical / Math Extensions (⎧, ⎪, ⎨, ⎷, ⌈, ⌉) */
      (c >= 0x2500 && c <= 0x257f))   /* Box Drawing (┌, ─, ┐, │) */
    {
      return 1;
    }

  /* Fullwidth / CJK Characters (Width = 2) */
  if ((c >= 0x1100 && c <= 0x115f) ||
      (c >= 0x2e80 && c <= 0xa4cf) ||
      (c >= 0xac00 && c <= 0xd7a3) ||
      (c >= 0xf900 && c <= 0xfaff) ||
      (c >= 0xfe10 && c <= 0xfe19) ||
      (c >= 0xfe30 && c <= 0xfe6f) ||
      (c >= 0xff01 && c <= 0xff60) ||
      (c >= 0xffe0 && c <= 0xffe6) ||
      (c >= 0x20000 && c <= 0x3fffff))
    {
      return 2;
    }

  return 1;
#else
  /* Native Linux / POSIX glibc implementation */
  return wcwidth(ch);
#endif
}

/*
 * Prompt for a string of hex numbers separated by spaces.
 * Treat each hex number as a Unicode character, convert
 * it to UTF-8, and insert into the current buffer.
 */
#ifndef TEST
int
unicode (int f, int n, int k)
{
  int s, len, i;
  char buf[80];
  char *p;
  unsigned int c[40];
  int count;

  s = ereply ("Enter Unicode characters in hex: ", buf, sizeof (buf));
  if (s != TRUE)
    return s;
  for (p = buf, count = 0; p < &buf[sizeof (buf)] && *p != '\0'; p += len, ++count)
    {
      s = sscanf (p, " %x%n", &c[count], &len);
      if (s != 1)
        {
          eprintf("Illegal hex number: %s", p);
          return FALSE;
        }
    }
  for (i = 0; i < count; i++)
    {
      if (linsert (1, c[i], NULL) == FALSE)
        return FALSE;
    }
  return TRUE;
}
#endif
