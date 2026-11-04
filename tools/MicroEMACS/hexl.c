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
 * The layout of a dump is a property of its buffer: the bytes run in
 * groups of 1, 2, 4, 8 or 16, and a group reads either big-endian,
 * the file order, or little-endian, its bytes reversed; the ASCII
 * area always keeps the file order, and a group cut short by the end
 * of the file keeps it too.  The "hexl-group-size" and "hexl-endian"
 * commands change the layout of the dump in the current buffer at
 * runtime, re-laying its lines out under the new one with the bytes,
 * and any edits, kept.
 *
 * The file is read and written with raw stdio, as the ffopen layer
 * is line oriented and a binary file is not lines.  Blank byte slots
 * pad the last line, so every line of a dump is the same width and a
 * column of the display is always the same byte.  The ASCII area is
 * display only; saving reads the hex.  A dump holds the whole file
 * as lines of text, so a file bigger than HEXLLIMIT bytes is
 * refused; windowing a dump over a bigger file is a design note in
 * TODO/hexl-mode.md.
 *
 * The dump line layout, in columns.  The hex area gives each byte
 * three columns, so it is 48 wide in every layout and the ASCII
 * area, and the line width, never move:
 *   0- 7   offset, eight hex digits
 *   8      ':'
 *   9      ' '
 *  10-57   the hex area, sixteen bytes
 *  58-59   ' '
 *  60-75   the ASCII area, '.' for a byte that is not printable
 * A dump opens in the GNU layout, the bytes spaced as pairs in two
 * groups of eight.  A group size set at run time packs each group
 * into one hex number, half its padding either side, as xxd and
 * fileview.c show their groups - a word, a dword, a quadword or an
 * octaword; groups of one are the bytes as pairs.
 */

#define HEXLWIDTH       76      /* dump line width, any layout   */
#define HEXLASCII       60      /* ASCII area first column       */
#define HEXLLIMIT       (8L * 1024L * 1024L)
                                /* biggest file dumped whole     */

static const char hexldigit[] = "0123456789abcdef";

static int hexlpend = -1;       /* High nibble typed, or -1.     */
static LINE *hexlpend_lp;       /* Line the nibble was typed on. */
static int hexlpend_k;          /* Byte it was typed for.        */

static int hexldefgroup = HEXLDEFGROUP;
                                /* Group size of new dumps.      */
static int hexldefendian = FALSE;
                                /* New dumps read big-endian.    */
static int hexldefpacked = FALSE;
                                /* New dumps pack their groups.  */

static int hexlhexcol (int d, int group, int packed);
static int hexldisp (int i, int n, int group, int endian);
static int hexllinebytes (LINE *lp, int group, int packed);
static void hexlmakeline (char *buf, unsigned long offset,
                          const uchar *bytes, int n,
                          int group, int endian, int packed);
static int hexlparseline (LINE *lp, uchar *bytes, int group, int endian,
                          int packed);
static int hexlsetbyte (int k, int byte, int n);
static void hexlforward (int d, int k, int ascii);
static int hexlhexval (int c);
static int hexlchange (int group, int endian, int packed);

/*
 * The column of the first hex digit of display slot d on a dump
 * line.  A slot is where a byte appears on the display; a group
 * reading little-endian shows its bytes in reverse, so the slot of a
 * byte is not always the byte's index.  The GNU layout the dump
 * opens in spaces the bytes as pairs; a group size set at run time
 * packs each group into one hex number with half its padding either
 * side.
 */
static int
hexlhexcol (int d, int group, int packed)
{
  if (packed == FALSE)           /* The GNU layout.     */
    return (10 + 3 * d + d / group);
  return (10 + group / 2 + d / group * 3 * group + 2 * (d % group));
}

/*
 * The display slot of byte i of a line holding n bytes: the file
 * order for a big-endian layout, or reversed within each group for a
 * little-endian one.  A group cut short by the end of the file keeps
 * the file order, so the blank slots still pad the line's end.  The
 * mapping is its own inverse: given a slot it returns the byte.
 */
static int
hexldisp (int i, int n, int group, int endian)
{
  int base = i - i % group;
  int size = group;

  if (base + size > n)
    size = n - base;            /* A short group at the end. */
  if (endian == FALSE || size < 2)
    return (i);
  return (base + size - 1 - (i - base));
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
 * The number of file bytes a dump line holds: its blank slots pad
 * the file's end, so the first of them ends the count.
 */
static int
hexllinebytes (LINE *lp, int group, int packed)
{
  int d;

  for (d = 0; d < HEXLBYTES; ++d)
    if (hexlhexval (lgetc (lp, hexlhexcol (d, group, packed))) < 0)
      break;
  return (d);
}

/*
 * Write one dump line into buf, holding n bytes of the file at the
 * given offset.  Missing bytes leave blank slots, so the line keeps
 * the full width.
 */
static void
hexlmakeline (char *buf, unsigned long offset, const uchar *bytes,
              int n, int group, int endian, int packed)
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
      col = hexlhexcol (hexldisp (i, n, group, endian), group, packed);
      buf[col] = hexldigit[bytes[i] >> 4];
      buf[col + 1] = hexldigit[bytes[i] & 0xf];
      buf[HEXLASCII + i] = (bytes[i] >= 0x20 && bytes[i] < 0x7f)
                            ? bytes[i] : '.';
    }
}

/*
 * Read a dump line's bytes into bytes.  Return the number of bytes
 * on the line, or -1 if the line is not a dump line: the wrong
 * width, a broken layout, or a byte after the blank slots that end
 * the file.  The line is rebuilt from its bytes and compared, so the
 * layout apart from the ASCII area must be exact.
 */
static int
hexlparseline (LINE *lp, uchar *bytes, int group, int endian, int packed)
{
  char line[HEXLWIDTH + 1];
  const char *text;
  unsigned long offset;
  int d, col, n;

  if (llength (lp) != HEXLWIDTH)
    return (-1);
  text = (const char *) lgets (lp);
  if (text[8] != ':' || text[9] != ' ')
    return (-1);
  for (d = 0; d < 8; ++d)
    if (hexlhexval (text[d]) < 0)
      return (-1);
  n = 0;
  while (n < HEXLBYTES)
    {
      col = hexlhexcol (n, group, packed);
      if (hexlhexval (text[col]) >= 0 && hexlhexval (text[col + 1]) >= 0)
        ++n;
      else if (text[col] == ' ' && text[col + 1] == ' ')
        break;                  /* First blank slot.    */
      else
        return (-1);
    }
  for (d = n; d < HEXLBYTES; ++d)
    {
      col = hexlhexcol (d, group, packed);
      if (text[col] != ' ' || text[col + 1] != ' ')
        return (-1);            /* A byte past the blanks. */
    }
  for (d = 0; d < n; ++d)
    {
      col = hexlhexcol (d, group, packed);
      bytes[hexldisp (d, n, group, endian)]
          = (uchar) ((hexlhexval (text[col]) << 4) | hexlhexval (text[col + 1]));
    }
  offset = 0;
  for (d = 0; d < 8; ++d)
    offset = offset * 16 + hexlhexval (text[d]);
  hexlmakeline (line, offset, bytes, n, group, endian, packed);
  if (memcmp (line, text, (size_t) HEXLASCII) != 0)
    return (-1);
  return (n);
}

/*
 * Write byte k of the line dot is on, into the hex pair and, when it
 * changes, the ASCII area.  The edits go through ldelete and
 * linsert, so undo and the read-only flag hold as for text.
 */
static int
hexlsetbyte (int k, int byte, int n)
{
  char buf[2];
  int group = curbp->b_hexlgroup;
  int endian = curbp->b_hexlendian;

  curwp->w_dot.o = hexlhexcol (hexldisp (k, n, group, endian), group,
                               curbp->b_hexlpacked);
  buf[0] = hexldigit[byte >> 4];
  buf[1] = hexldigit[byte & 0xf];
  if (ldelete (2, FALSE) == FALSE)
    return (FALSE);
  linsert (2, 0, buf);
  buf[0] = (byte >= 0x20 && byte < 0x7f) ? byte : '.';
  if (lgetc (curwp->w_dot.p, HEXLASCII + k) != buf[0])
    {
      curwp->w_dot.o = HEXLASCII + k;
      if (ldelete (1, FALSE) == FALSE)
        return (FALSE);
      linsert (1, 0, buf);
    }
  return (TRUE);
}

/*
 * Step dot past the byte just edited: display slot d+1 in the hex
 * area, so the dot always moves right across the dump, or the next
 * byte in the ASCII area.
 */
static void
hexlforward (int d, int k, int ascii)
{
  int group = curbp->b_hexlgroup;

  if (ascii)
    {
      if (k + 1 < HEXLBYTES)
        curwp->w_dot.o = HEXLASCII + k + 1;
      else
        {
          curwp->w_dot.p = lforw (curwp->w_dot.p);
          curwp->w_dot.o = HEXLASCII;
        }
    }
  else
    {
      if (d + 1 < HEXLBYTES)
        curwp->w_dot.o = hexlhexcol (d + 1, group, curbp->b_hexlpacked);
      else
        {
          curwp->w_dot.p = lforw (curwp->w_dot.p);
          curwp->w_dot.o = hexlhexcol (0, group, curbp->b_hexlpacked);
        }
    }
}

/*
 * Fill the current buffer, which "bclear" has emptied, with the dump
 * of the file "fname", and make the buffer a hexl buffer with the
 * layout new dumps take.  Shared by the "hexl-find-file" command and
 * the mainline "-H" option.
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
  long len;
  size_t got;

  if ((fp = fopen (fname, "rb")) == NULL)
    {
      eprintf ("[No such file \"%s\"]", fname);
      return (FALSE);
    }
  if (fseek (fp, 0, SEEK_END) != 0 || (len = ftell (fp)) < 0
      || fseek (fp, 0, SEEK_SET) != 0)
    {				/* A pipe, or worse.    */
      fclose (fp);
      eprintf ("[The file will not seek, so it cannot be dumped]");
      return (FALSE);
    }
  if (len > HEXLLIMIT)
    {				/* A dump holds the     */
      fclose (fp);		/* whole file as text.  */
      eprintf ("[File is %l bytes; a dump holds up to %l]", len,
               HEXLLIMIT);
      return (FALSE);
    }
  bp = curbp;
  bp->b_hexlgroup = hexldefgroup;
  bp->b_hexlendian = hexldefendian;
  bp->b_hexlpacked = hexldefpacked;
  lp2 = firstline (bp);         /* The empty line bclear  */
                                /* left, as in "readin".  */
  offset = 0;
  while ((got = fread (bytes, 1, HEXLBYTES, fp)) > 0)
    {
      hexlmakeline (line, offset, bytes, (int) got,
                    bp->b_hexlgroup, bp->b_hexlendian, bp->b_hexlpacked);
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
          wp->w_dot.o = llength (lp1) == HEXLWIDTH
                        ? hexlhexcol (0, bp->b_hexlgroup, bp->b_hexlpacked) : 0;
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
 * Change the layout of the dump in the current buffer - the group
 * size, the byte order within a group, or the packing of the groups,
 * or any two of them - by parsing its lines under the old layout and
 * writing them under the new one.  Bytes not yet saved are kept, but
 * the undo history is not: its records speak the old layout.  The
 * offsets are rewritten in step, as the save reads only the bytes.
 */
static int
hexlchange (int group, int endian, int packed)
{
  BUFFER *bp;
  EWINDOW *wp;
  LINE *lp, *next, *newlp, *first;
  uchar *bytes;		/* The dump's bytes, line by line. */
  int *counts;		/* Bytes on each line.            */
  char line[HEXLWIDTH + 1];
  unsigned long offset;
  int lineno, count, i, n;

  bp = curbp;
  if ((bp->b_flag & BFHEXL) == 0)
    {
      eprintf ("[Not a hexl buffer]");
      return (FALSE);
    }

  count = 0;
  for (lp = firstline (bp); lp != bp->b_linep; lp = lforw (lp))
    {
      if (llength (lp) == 0 && lforw (lp) == bp->b_linep)
        break;			/* The trailing empty line. */
      ++count;
    }
  bytes = malloc ((size_t) count * HEXLBYTES + 1);
  counts = malloc (((size_t) count + 1) * sizeof (int));
  if (bytes == NULL || counts == NULL)
    {
      free (bytes);
      free (counts);
      eprintf ("Out of memory");
      return (FALSE);
    }

  lineno = 0;
  i = 0;
  for (lp = firstline (bp); lp != bp->b_linep; lp = lforw (lp))
    {
      if (llength (lp) == 0 && lforw (lp) == bp->b_linep)
        break;
      ++lineno;
      if ((n = hexlparseline (lp, bytes + i * HEXLBYTES,
                              bp->b_hexlgroup, bp->b_hexlendian,
                              bp->b_hexlpacked)) < 0)
        {
          free (bytes);
          free (counts);
          eprintf ("[Line %d is not a hexl dump line]", lineno);
          return (FALSE);
        }
      counts[i++] = n;
    }

  bp->b_hexlgroup = group;
  bp->b_hexlendian = endian;
  bp->b_hexlpacked = packed;
  killundo (bp);		/* Old records speak the old layout. */
  hexlpend = -1;

  first = NULL;
  offset = 0;
  for (lp = firstline (bp), i = 0; lp != bp->b_linep; lp = next, ++i)
    {
      if (llength (lp) == 0 && lforw (lp) == bp->b_linep)
        break;
      next = lforw (lp);
      hexlmakeline (line, offset, bytes + i * HEXLBYTES, counts[i],
                    group, endian, packed);
      if ((newlp = lallocx (HEXLWIDTH)) == NULL)
        {
          free (bytes);		/* Half converted; the save  */
          free (counts);	/* will refuse the mismatch. */
          eprintf ("Out of memory");
          return (FALSE);
        }
      lputs (newlp, line, HEXLWIDTH);
      newlp->l_bp = lp->l_bp;	/* Replace lp with newlp. */
      newlp->l_fp = lp->l_fp;
      lp->l_bp->l_fp = newlp;
      lp->l_fp->l_bp = newlp;
      free (lp);		/* As "bclear" frees one. */
      if (first == NULL)
        first = newlp;
      offset += (unsigned long) counts[i];
    }
  free (bytes);
  free (counts);

  if (first != NULL)
    {
      ALLWIND (wp)
        {
          if (wp->w_bufp == bp)
            {
              wp->w_linep = first;
              wp->w_dot.p = first;
              wp->w_dot.o = hexlhexcol (0, group, packed);
              wp->w_mark.p = NULL;
              wp->w_mark.o = 0;
              wp->w_flag |= WFMODE | WFHARD;
            }
        }
      bp->b_dot.p = first;	/* If no window shows it. */
      bp->b_mark.p = NULL;
      bp->b_mark.o = 0;
    }
  eprintf ("[Dump relaid: groups of %d, %s-endian]", group,
           endian != FALSE ? "little" : "big");
  return (TRUE);
}

/*
 * Set the group size of the dump - the bytes run in groups of 1, 2,
 * 4, 8 or 16, each group packed into one hex number.  A prefix
 * argument is taken as the size; the dump in the current buffer is
 * re-laid out under it, and new dumps take it.
 */
int
hexlsetgroup (int f, int n, int k)
{
  char buf[NLINE];
  int group;

  (void) k;
  if (f != FALSE)
    group = n < 0 ? -n : n;
  else
    {
      if ((n = ereply ("Group size (1/2/4/8/16): ", buf, sizeof (buf)))
          != TRUE)
        return (n);
      if (buf[0] == 0)
        return (TRUE);
      group = atoi (buf);
    }
  if (group != 1 && group != 2 && group != 4 && group != 8
      && group != 16)
    {
      eprintf ("[Group size is 1, 2, 4, 8 or 16]");
      return (FALSE);
    }
  hexldefgroup = group;
  hexldefpacked = TRUE;
  if ((curbp->b_flag & BFHEXL) == 0
      || (group == curbp->b_hexlgroup && curbp->b_hexlpacked != FALSE))
    return (TRUE);
  return (hexlchange (group, curbp->b_hexlendian, TRUE));
}

/*
 * Switch the byte order the groups of a dump read in, big-endian
 * (the file order) or little-endian (reversed within each group).
 * A prefix argument of 0 reads big-endian and any other reads
 * little-endian; without one, the order toggles.  The dump in the
 * current buffer is re-laid out, and new dumps take the order.
 */
int
hexlsetendian (int f, int n, int k)
{
  int endian;

  (void) k;
  if (f != FALSE)
    endian = n != 0;
  else if ((curbp->b_flag & BFHEXL) != 0)
    endian = !curbp->b_hexlendian;
  else
    endian = !hexldefendian;
  hexldefendian = endian;
  if ((curbp->b_flag & BFHEXL) == 0 || endian == curbp->b_hexlendian)
    return (TRUE);
  return (hexlchange (curbp->b_hexlgroup, endian, curbp->b_hexlpacked));
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
      if ((nb = hexlparseline (lp, bytes,
                               bp->b_hexlgroup, bp->b_hexlendian,
                               bp->b_hexlpacked)) < 0)
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
      nb = hexlparseline (lp, bytes, bp->b_hexlgroup, bp->b_hexlendian,
                          bp->b_hexlpacked);
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
  int group, endian, i;

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
  group = curbp->b_hexlgroup;
  endian = curbp->b_hexlendian;
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
  curwp->w_dot.o = hexlhexcol
      (hexldisp ((int) (addr % HEXLBYTES),
                 hexllinebytes (lp, group, curbp->b_hexlpacked),
                 group, endian), group, curbp->b_hexlpacked);
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
  int group, endian;
  int col, d, k, n, v, ascii, byte;

  group = curbp->b_hexlgroup;
  endian = curbp->b_hexlendian;
  lp = curwp->w_dot.p;
  if (lp == curbp->b_linep || llength (lp) != HEXLWIDTH
      || lgetc (lp, 8) != ':')
    {
      eprintf ("[Not a hexl dump line]");
      return (FALSE);
    }
  n = hexllinebytes (lp, group, curbp->b_hexlpacked);
  col = curwp->w_dot.o;
  if (col >= HEXLASCII && col < HEXLASCII + HEXLBYTES)
    {
      ascii = TRUE;
      k = col - HEXLASCII;
      d = k;			/* The ASCII area keeps file order. */
    }
  else
    {
      ascii = FALSE;
      for (d = 0; d < HEXLBYTES; ++d)
        if (col == hexlhexcol (d, group, curbp->b_hexlpacked)
            || col == hexlhexcol (d, group, curbp->b_hexlpacked) + 1)
          break;
      if (d >= HEXLBYTES)
        {
          eprintf ("[Dot is not on a byte]");
          return (FALSE);
        }
      k = hexldisp (d, n, group, endian);
    }

  if (c == '\t')
    {
      hexlpend = -1;
      hexlforward (d, k, ascii);
      return (TRUE);
    }

  /* The byte must be one the file holds, not a padding blank. */
  if (k >= n)
    {
      eprintf ("[That byte is past the end of the file]");
      return (FALSE);
    }

  if (ascii != FALSE)
    {
      hexlpend = -1;
      if (hexlsetbyte (k, c & 0xff, n) == FALSE)
        return (FALSE);
      hexlforward (d, k, ascii);
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
      curwp->w_dot.o = hexlhexcol (d, group, curbp->b_hexlpacked);
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
  if (hexlsetbyte (k, byte, n) == FALSE)
    return (FALSE);
  hexlforward (d, k, ascii);
  return (TRUE);
}
