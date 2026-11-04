/*
 * Filename hexl.c
 */

#include "def.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Byte editing in a hex dump, after the manner of GNU's hexl mode.
 * The buffer holds the dump itself: an eight digit offset, sixteen
 * bytes in hex, and the printable characters beside them, one line
 * per sixteen bytes of the file.  A byte is edited by typing over it
 * in either column - two hex digits in the hex area write the byte,
 * and a character in the ASCII area writes the character's value -
 * and both columns follow each other, so the dump stays a dump.
 * "hexl-save" parses the hex back out of the lines and writes the
 * file, refusing to save a line that is not a dump line, so damage
 * from commands that are not hexl aware cannot reach the file.
 *
 * The file is read and written with raw stdio, as the ffopen layer
 * is line oriented and a binary file is not lines.  Blank byte slots
 * pad the last line, so every line of a dump is the same width and a
 * column of the display is always the same byte.  The ASCII area is
 * display only; saving reads the hex.
 *
 * The dump line layout, in columns:
 *   0- 7   offset, eight hex digits
 *   8      ':'
 *   9      ' '
 *  10-33   bytes 0-7, as "xx " each
 *  34     ' ', separating the two groups of eight
 *  35-58   bytes 8-15, as "xx " each
 *  59     ' '
 *  60-75   the ASCII area, '.' for a byte that is not printable
 */

#define HEXLBYTES       16      /* bytes on a dump line          */
#define HEXLWIDTH       76      /* columns in a dump line        */

static const char hexldigit[] = "0123456789abcdef";

static int hexlpend = -1;       /* High nibble typed, or -1.     */
static LINE *hexlpend_lp;       /* Line the nibble was typed on. */
static int hexlpend_k;          /* Byte it was typed for.        */

static void hexlmakeline (char *buf, unsigned long offset,
                          const uchar *bytes, int n);
static int hexlparseline (LINE *lp, uchar *bytes);
static int hexlsetbyte (int k, int byte);
static void hexlforward (int k, int ascii);
static int hexlhexcol (int k);
static int hexlhexval (int c);
static int hexlfill (const char *fname);

/*
 * The column of the first hex digit of byte k on a dump line.
 */
static int
hexlhexcol (int k)
{
  return (10 + 3 * k + (k >= 8 ? 1 : 0));
}

/*
 * The value of a hex digit, or -1 if c is not one.
 */
static int
hexlhexval (int c)
{
  if (c >= '0' && c <= '9')
    return (c - '0');
  if (c >= 'a' && c <= 'f')
    return (c - 'a' + 10);
  if (c >= 'A' && c <= 'F')
    return (c - 'A' + 10);
  return (-1);
}

/*
 * Write one dump line into buf, holding n bytes of the file at the
 * given offset.  Missing bytes leave blank slots, so the line keeps
 * the full width.
 */
static void
hexlmakeline (char *buf, unsigned long offset, const uchar *bytes, int n)
{
  int i, col;

  memset (buf, ' ', HEXLWIDTH);
  for (i = 7; i >= 0; --i)
    {
      buf[i] = hexldigit[offset & 0xf];
      offset >>= 4;
    }
  buf[8] = ':';
  for (i = 0; i < n; ++i)
    {
      col = hexlhexcol (i);
      buf[col] = hexldigit[bytes[i] >> 4];
      buf[col + 1] = hexldigit[bytes[i] & 0xf];
      buf[60 + i] = (bytes[i] >= 0x20 && bytes[i] < 0x7f) ? bytes[i] : '.';
    }
}

/*
 * Read a dump line's bytes into bytes.  Return the number of bytes
 * on the line, or -1 if the line is not a dump line: the wrong
 * width, a broken layout, or a byte after the blank slots that end
 * the file.
 */
static int
hexlparseline (LINE *lp, uchar *bytes)
{
  const char *text;
  int k, col, hi, lo, n;

  if (llength (lp) != HEXLWIDTH)
    return (-1);
  text = (const char *) lgets (lp);
  if (text[8] != ':' || text[9] != ' ' || text[34] != ' '
      || text[59] != ' ')
    return (-1);
  for (k = 0; k < 8; ++k)
    if (hexlhexval (text[k]) < 0)
      return (-1);
  n = HEXLBYTES;
  for (k = 0; k < HEXLBYTES; ++k)
    {
      col = hexlhexcol (k);
      hi = hexlhexval (text[col]);
      lo = hexlhexval (text[col + 1]);
      if (hi >= 0 && lo >= 0)
        {
          if (n != HEXLBYTES)  /* A byte past the blanks. */
            return (-1);
          bytes[k] = (uchar) ((hi << 4) | lo);
        }
      else if (hi < 0 && lo < 0
               && text[col] == ' ' && text[col + 1] == ' ')
        {
          if (n == HEXLBYTES)
            n = k;             /* First blank slot.    */
        }
      else
        return (-1);
    }
  return (n);
}

/*
 * Write byte k of the line dot is on, into the hex pair and, when it
 * changes, the ASCII area.  The edits go through ldelete and
 * linsert, so undo and the read-only flag hold as for text.
 */
static int
hexlsetbyte (int k, int byte)
{
  char buf[2];

  curwp->w_dot.o = hexlhexcol (k);
  buf[0] = hexldigit[byte >> 4];
  buf[1] = hexldigit[byte & 0xf];
  if (ldelete (2, FALSE) == FALSE)
    return (FALSE);
  linsert (2, 0, buf);
  buf[0] = (byte >= 0x20 && byte < 0x7f) ? byte : '.';
  if (lgetc (curwp->w_dot.p, 60 + k) != buf[0])
    {
      curwp->w_dot.o = 60 + k;
      if (ldelete (1, FALSE) == FALSE)
        return (FALSE);
      linsert (1, 0, buf);
    }
  return (TRUE);
}

/*
 * Step dot to byte k+1, in the area dot was edited in.
 */
static void
hexlforward (int k, int ascii)
{
  if (k + 1 < HEXLBYTES)
    curwp->w_dot.o = ascii ? 61 + k : hexlhexcol (k + 1);
  else
    {
      curwp->w_dot.p = lforw (curwp->w_dot.p);
      curwp->w_dot.o = ascii ? 60 : 10;
    }
}

/*
 * Fill the current buffer, which "bclear" has emptied, with the dump
 * of the file "fname", and make the buffer a hexl buffer.  Shared by
 * the "hexl-find-file" command and the mainline "-H" option.
 */
static int
hexlfill (const char *fname)
{
  BUFFER *bp;
  EWINDOW *wp;
  LINE *lp1, *lp2;
  FILE *fp;
  char line[HEXLWIDTH + 1];
  uchar bytes[HEXLBYTES];
  unsigned long offset;
  size_t got;

  if ((fp = fopen (fname, "rb")) == NULL)
    {
      eprintf ("[No such file \"%s\"]", fname);
      return (FALSE);
    }
  bp = curbp;
  lp2 = firstline (bp);         /* The empty line bclear  */
                                /* left, as in "readin".  */
  offset = 0;
  while ((got = fread (bytes, 1, HEXLBYTES, fp)) > 0)
    {
      hexlmakeline (line, offset, bytes, (int) got);
      if ((lp1 = lallocx (HEXLWIDTH)) == NULL)
        {
          fclose (fp);
          return (FALSE);
        }
      lp1->l_bp = lp2->l_bp;    /* Insert lp1           */
      lp1->l_fp = lp2;          /*  before lp2.         */
      lp2->l_bp->l_fp = lp1;
      lp2->l_bp = lp1;
      lputs (lp1, line, HEXLWIDTH);
      offset += (unsigned long) got;
    }
  if (ferror (fp))
    {
      fclose (fp);
      eprintf ("[Read error]");
      return (FALSE);
    }
  fclose (fp);
  strcpy (bp->b_fname, fname);
  bp->b_flag |= BFHEXL;
#if BACKUP
  bp->b_flag |= BFBAK;          /* Need a backup.       */
#endif
  createmode ("hexl");
  lp1 = firstline (bp);
  ALLWIND (wp)                  /* As "readin" sets them. */
    {
      if (wp->w_bufp == bp)
        {
          wp->w_linep = lp1;
          wp->w_dot.p = lp1;
          wp->w_dot.o = llength (lp1) == HEXLWIDTH ? hexlhexcol (0) : 0;
          wp->w_flag |= WFMODE | WFHARD;
        }
    }
  bp->b_dot.p = lp1;            /* If no window shows it. */
  hexlpend = -1;
  eprintf ("[Read %l bytes]", offset);
  return (TRUE);
}

/*
 * Read a file into a hexl dump buffer.  A file already dumped is
 * revisited, as "visit_file" revisits its buffer.  The buffer takes
 * a mode named "hexl", which the mode line shows.
 */
int
hexlfind (int f, int n, int k)
{
  BUFFER *bp;
  EWINDOW *wp;
  char fname[NFILEN];
  char bname[NBUFN];
  char *expanded;

  (void) f;
  (void) k;
  if ((n = egetfname ("Hexl file: ", fname, NFILEN)) != TRUE)
    return (n);
  adjustcase (fname);
  expanded = fftilde (fname);

  ALLBUF (bp)
    {
      if ((bp->b_flag & BFHEXL) != 0
          && strcmp (bp->b_fname, expanded) == 0)
        {
          addwind (curwp, -1);
          strcpy (oldbufn, curbp->b_bname);
          curbp = bp;
          curwp->w_bufp = bp;
          addwind (curwp, 1);
          if (bp->b_nwnd != 1)
            ALLWIND (wp)
            {
              if (wp != curwp && wp->w_bufp == bp)
                {
                  curwp->w_dot = wp->w_dot;
                  curwp->w_mark = wp->w_mark;
                  break;
                }
            }
          curwp->w_flag |= WFMODE | WFHARD;
          if (kbdmop == NULL)
            eprintf ("[Old buffer]");
          return (TRUE);
        }
    }

  makename (bname, expanded);
  unqname (bname, NBUFN);
  if ((bp = bfind (bname, TRUE)) == NULL)
    {
      eprintf ("Cannot create buffer");
      return (FALSE);
    }
  if (bclear (bp) != TRUE)      /* May refuse to discard. */
    return (FALSE);
  swbuffer (bp);
  return (hexlfill (expanded));
}

/*
 * Read a file into the current buffer as a hexl dump, for the
 * mainline "-H" option.  The buffer is the one "bufinit" made for
 * the file; it is cleared as "readin" clears one, then filled with
 * the dump.
 */
int
hexlread (const char *fname)
{
  if (bclear (curbp) != TRUE)   /* Might be old.        */
    return (FALSE);
  return (hexlfill (fname));
}

/*
 * Parse the hex out of the dump lines and write the file, as raw
 * bytes.  Every line is checked before the file is touched, so a
 * line damaged by a command that is not hexl aware stops the save
 * with the file as it was.
 */
int
hexlsave (int f, int n, int k)
{
  BUFFER *bp;
  LINE *lp;
  FILE *fp;
  uchar bytes[HEXLBYTES];
  unsigned long total;
  int lineno, nb;

  (void) f;
  (void) k;
  bp = curbp;
  if ((bp->b_flag & BFHEXL) == 0)
    {
      eprintf ("[Not a hexl buffer]");
      return (FALSE);
    }
  if ((bp->b_flag & BFCHG) == 0)
    return (TRUE);
  if (bp->b_fname[0] == 0)
    {
      eprintf ("No file name");
      return (FALSE);
    }
  hexlpend = -1;
#if BACKUP
  if (bflag == TRUE && (bp->b_flag & BFBAK) != 0)
    {
      n = fbackupfile (bp->b_fname);
      if (n == ABORT)          /* Hard error.          */
        return (n);
      if (n == FALSE           /* Softer error.        */
          && (n = eyesno ("Backup error, save anyway")) != TRUE)
        return (n);
    }
#endif

  lineno = 0;
  total = 0;
  lp = firstline (bp);
  while (lp != bp->b_linep)
    {
      /* The empty line that ends the buffer is the final newline,
       * not a damaged dump line. */
      if (llength (lp) == 0 && lforw (lp) == bp->b_linep)
        break;
      ++lineno;
      if ((nb = hexlparseline (lp, bytes)) < 0)
        {
          eprintf ("[Line %d is not a hexl dump line]", lineno);
          return (FALSE);
        }
      total += (unsigned long) nb;
      lp = lforw (lp);
    }

  if ((fp = fopen (bp->b_fname, "wb")) == NULL)
    {
      eprintf ("[Cannot open \"%s\" for writing]", bp->b_fname);
      return (FALSE);
    }
  lp = firstline (bp);
  while (lp != bp->b_linep && ferror (fp) == 0)
    {
      nb = hexlparseline (lp, bytes);
      if (nb > 0 && fwrite (bytes, 1, (size_t) nb, fp) != (size_t) nb)
        break;
      lp = lforw (lp);
    }
  if (ferror (fp) || fclose (fp) != 0)
    {
      eprintf ("[Write error]");
      return (FALSE);
    }
  bp->b_flag &= ~(BFCHG | BFBAK);
  updatemode ();                /* Update mode lines.   */
  setundochanged ();
  eprintf ("[Wrote %l bytes]", total);
  return (TRUE);
}

/*
 * Move dot to a byte of the dump by its file offset.  The address
 * reads as decimal, or as hex with a 0x prefix, and a prefix
 * argument is taken as the address.
 */
int
hexlgoto (int f, int n, int k)
{
  LINE *lp;
  char buf[NLINE];
  unsigned long addr;
  int i;

  (void) k;
  if ((curbp->b_flag & BFHEXL) == 0)
    {
      eprintf ("[Not a hexl buffer]");
      return (FALSE);
    }
  if (f != FALSE)
    addr = (unsigned long) (n < 0 ? -n : n);
  else
    {
      if ((i = ereply ("Goto address: ", buf, sizeof (buf))) != TRUE)
        return (i);
      if (buf[0] == 0)
        return (TRUE);
      addr = strtoul (buf, NULL, 0);
    }
  hexlpend = -1;
  lp = firstline (curbp);
  for (i = (int) (addr / HEXLBYTES); i > 0; --i)
    {
      if (lp == curbp->b_linep)
        break;
      lp = lforw (lp);
    }
  if (lp == curbp->b_linep)
    {
      eprintf ("[Address is past the end of the dump]");
      return (FALSE);
    }
  curwp->w_dot.p = lp;
  curwp->w_dot.o = hexlhexcol ((int) (addr % HEXLBYTES));
  curwp->w_flag |= WFMOVE;
  return (TRUE);
}

/*
 * A character typed into a hexl buffer.  A tab steps to the next
 * byte.  A hex digit in the hex area is one nibble of the byte
 * there; two of them write it, and the first stays to be seen until
 * the second arrives.  Any other character in the hex area is
 * refused, while one in the ASCII area writes its own value as the
 * byte.  Dot steps to the next byte after a byte is written.
 */
int
hexlself (int c)
{
  LINE *lp;
  char buf[2];
  int col, k, v, ascii, byte;

  lp = curwp->w_dot.p;
  if (lp == curbp->b_linep || llength (lp) != HEXLWIDTH
      || lgetc (lp, 8) != ':')
    {
      eprintf ("[Not a hexl dump line]");
      return (FALSE);
    }
  col = curwp->w_dot.o;
  if (col >= 60 && col < 76)
    {
      ascii = TRUE;
      k = col - 60;
    }
  else if (col >= 10 && col < 59)
    {
      ascii = FALSE;
      k = col < 35 ? (col - 10) / 3 : 8 + (col - 35) / 3;
    }
  else
    {
      eprintf ("[Dot is not on a byte]");
      return (FALSE);
    }

  if (c == '\t')
    {
      hexlpend = -1;
      hexlforward (k, ascii);
      return (TRUE);
    }

  /* The byte must be one the file holds, not a padding blank. */
  if (hexlhexval (lgetc (lp, hexlhexcol (k))) < 0)
    {
      eprintf ("[That byte is past the end of the file]");
      return (FALSE);
    }

  if (ascii != FALSE)
    {
      hexlpend = -1;
      if (hexlsetbyte (k, c & 0xff) == FALSE)
        return (FALSE);
      hexlforward (k, ascii);
      return (TRUE);
    }

  if ((v = hexlhexval (c)) < 0)
    {
      eprintf ("[Hex digits here; a character goes in the ASCII area]");
      return (FALSE);
    }
  if (hexlpend < 0 || hexlpend_lp != lp || hexlpend_k != k)
    {
      hexlpend = v;
      hexlpend_k = k;
      curwp->w_dot.o = hexlhexcol (k);
      buf[0] = (char) c;
      if (ldelete (1, FALSE) == FALSE)
        return (FALSE);
      linsert (1, 0, buf);
      hexlpend_lp = curwp->w_dot.p;     /* The line may move.   */
      return (TRUE);
    }
  /* The high nibble holds: the byte is written and dot steps on. */
  byte = (hexlpend << 4) | v;
  hexlpend = -1;
  if (hexlsetbyte (k, byte) == FALSE)
    return (FALSE);
  hexlforward (k, ascii);
  return (TRUE);
}
