/*
 * bb.h - the sliver of busybox's libbb that vi.c leans on, written
 * for building the editor as a single Linux application.  The
 * helpers keep their busybox names and semantics so vi.c stays
 * byte-for-byte the busybox editor; what is vendored here is the
 * small set it actually calls: the x* allocators, a few string and
 * read utilities, the getopt32 option parser, and read_key, the
 * escape-sequence decoder that returns KEYCODE_* values.
 *
 * Busybox is Copyright (C) many authors, Licensed under GPLv2 or
 * later, see file LICENSE in the busybox source tree.
 */
#ifndef BB_H
#define BB_H

#define _GNU_SOURCE 1

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <poll.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

/* ------------------------------------------------------------------ */
/* The feature set, standing in for busybox's CONFIG_* machinery: the  */
/* practical features on, CRASHME off as vi.c itself forces, and no   */
/* locale support - the editor is not Unicode-capable.                 */
/* ------------------------------------------------------------------ */
#define ENABLE_FEATURE_VI_8BIT            1
#define ENABLE_FEATURE_VI_ASK_TERMINAL     1
#define ENABLE_FEATURE_VI_COLON            1
#define ENABLE_FEATURE_VI_COLON_EXPAND     1
#define ENABLE_FEATURE_VI_CRASHME          0
#define ENABLE_FEATURE_VI_DOT_CMD          1
#define ENABLE_FEATURE_VI_READONLY         1
/* busybox itself defaults this one off: the code is written against
 * the GNU regex API (re_syntax_options, re_search, REG_STARTEND),
 * which glibc has and musl has not.  Off keeps the search and the
 * :s substitute on the plain literal-match path, and the static
 * publish build the same editor as the local one. */
#define ENABLE_FEATURE_VI_REGEX_SEARCH     0
#define ENABLE_FEATURE_VI_SEARCH           1
#define ENABLE_FEATURE_VI_SET              1
#define ENABLE_FEATURE_VI_SETOPTS          1
#define ENABLE_FEATURE_VI_UNDO             1
#define ENABLE_FEATURE_VI_UNDO_QUEUE       1
#define ENABLE_FEATURE_VI_USE_SIGNALS      1
#define ENABLE_FEATURE_VI_VERBOSE_STATUS   1
#define ENABLE_FEATURE_VI_WIN_RESIZE       1
#define ENABLE_FEATURE_VI_YANKMARK         1
#define ENABLE_LOCALE_SUPPORT              0

/* The numeric knobs, the values busybox configures by default. */
#define CONFIG_FEATURE_VI_MAX_LEN          4096
#define CONFIG_FEATURE_VI_UNDO_QUEUE_MAX   256

/* busybox's IF_FEATURE_xxx(arg) expands to arg when the feature is
 * on and to nothing when it is off; vi.c uses it to fold the
 * conditional parts of tables and calls out of the source. */
#define IF_FEATURE_VI_8BIT(...)            __VA_ARGS__
#define IF_FEATURE_VI_ASK_TERMINAL(...)     __VA_ARGS__
#define IF_FEATURE_VI_COLON(...)            __VA_ARGS__
#define IF_FEATURE_VI_COLON_EXPAND(...)     __VA_ARGS__
#define IF_FEATURE_VI_CRASHME(...)
#define IF_FEATURE_VI_DOT_CMD(...)          __VA_ARGS__
#define IF_FEATURE_VI_READONLY(...)         __VA_ARGS__
#define IF_FEATURE_VI_REGEX_SEARCH(...)
#define IF_FEATURE_VI_SEARCH(...)           __VA_ARGS__
#define IF_FEATURE_VI_SET(...)              __VA_ARGS__
#define IF_FEATURE_VI_SETOPTS(...)          __VA_ARGS__
#define IF_FEATURE_VI_UNDO(...)             __VA_ARGS__
#define IF_FEATURE_VI_UNDO_QUEUE(...)       __VA_ARGS__
#define IF_FEATURE_VI_USE_SIGNALS(...)      __VA_ARGS__
#define IF_FEATURE_VI_VERBOSE_STATUS(...)   __VA_ARGS__
#define IF_FEATURE_VI_WIN_RESIZE(...)       __VA_ARGS__
#define IF_FEATURE_VI_YANKMARK(...)         __VA_ARGS__

/* ------------------------------------------------------------------ */
/* Small busybox vocabulary used by vi.c.  ESC and write1 are vi.c's  */
/* own; the rest stand in for libbb.h.                                 */
/* ------------------------------------------------------------------ */
#define FALSE         0
#define TRUE          1
#define UNUSED_PARAM __attribute__ ((__unused__))
#define ALWAYS_INLINE inline __attribute__ ((always_inline))
#define ALIGN1
#define fflush_all() fflush(NULL)

typedef int smallint;
typedef unsigned smalluint;

/* vi.c keeps its whole state in one struct globals block, reached
 * through this pointer; vi.c's G macro does the field access. */
struct globals;
static struct globals *ptr_to_globals;
#define SET_PTR_TO_GLOBALS(x) (ptr_to_globals = (x))

#define ARRAY_SIZE(x) ((unsigned) (sizeof (x) / sizeof ((x)[0])))

/* glibc's printf knows %m, so errno's text needs no argument. */
#define STRERROR_FMT   "%m"
#define STRERROR_ERRNO

#define BB_VER "1.39.0.git"

static void fputs_stdout (const char *s)
{
  fputs (s, stdout);
}

static char *last_char_is (const char *s, int c)
{
  if (s[0] == '\0')
    return NULL;
  while (s[1] != '\0')
    s++;
  return *s == (char) c ? (char *) s : NULL;
}

/* whitespace is "\t\n\v\f\r" (9..13) and space, no locale surprises */
static char *skip_whitespace (const char *s)
{
  while (*s == ' ' || (unsigned char) (*s - 9) <= (13 - 9))
    s++;
  return (char *) s;
}

static char *skip_non_whitespace (const char *s)
{
  while (*s != '\0' && *s != ' ' && (unsigned char) (*s - 9) > (13 - 9))
    s++;
  return (char *) s;
}

typedef struct llist_t
{
  struct llist_t *next;
  char *data;
}
llist_t;

/* ------------------------------------------------------------------ */
/* Fatal helpers: the x* allocators and wrappers that never return a   */
/* failure, exiting with a message instead.                            */
/* ------------------------------------------------------------------ */
static void __attribute__ ((__noreturn__)) bb_die (const char *msg)
{
  fprintf (stderr, "vi: %s\n", msg);
  exit (1);
}

static void *xmalloc (size_t n)
{
  void *p = malloc (n);
  if (p == NULL)
    bb_die ("out of memory");
  return p;
}

static void *xzalloc (size_t n)
{
  void *p = xmalloc (n);
  memset (p, 0, n);
  return p;
}

static void *xrealloc (void *p, size_t n)
{
  p = realloc (p, n);
  if (p == NULL)
    bb_die ("out of memory");
  return p;
}

static char *xstrdup (const char *s)
{
  char *p = strdup (s);
  if (p == NULL)
    bb_die ("out of memory");
  return p;
}

static char *xstrndup (const char *s, size_t n)
{
  char *p = strndup (s, n);
  if (p == NULL)
    bb_die ("out of memory");
  return p;
}

static char *xasprintf (const char *fmt, ...)
{
  va_list ap;
  char *p;

  va_start (ap, fmt);
  if (vasprintf (&p, fmt, ap) < 0)
    bb_die ("out of memory");
  va_end (ap);
  return p;
}

/* Build the replacement first - the format may print from the very
 * buffer being replaced - then free the old one. */
static char *xasprintf_and_free (char *allocated, const char *fmt, ...)
{
  va_list ap;
  char *p;

  va_start (ap, fmt);
  if (vasprintf (&p, fmt, ap) < 0)
    bb_die ("out of memory");
  va_end (ap);
  free (allocated);
  return p;
}

#define xasprintf_inplace(allocated, ...) \
  ((allocated) = xasprintf_and_free ((allocated), __VA_ARGS__))

/* ------------------------------------------------------------------ */
/* Character and string utilities.                                     */
/* ------------------------------------------------------------------ */
static int bb_putchar (int ch)
{
  return putchar (ch);
}

/* busybox's unsigned parse: ULONG_MAX, not a partially parsed value,
 * when the text is empty, negative, trailing-dirty, or out of range. */
static unsigned long bb_strtou (const char *arg, char **endp, int base)
{
  unsigned long v;
  char *end;

  errno = 0;
  v = strtoul (arg, &end, base);
  if (endp != NULL)
    *endp = end;
  if (arg[0] == '\0' || arg[0] == '-' || *end != '\0' || errno == ERANGE)
    return ULONG_MAX;
  return v;
}

static char *safe_strncpy (char *dst, const char *src, size_t size)
{
  if (size == 0)
    return dst;
  dst[--size] = '\0';
  return strncpy (dst, src, size);
}

static char *concat_path_file (const char *path, const char *filename)
{
  size_t len;

  if (path == NULL)
    path = "";
  len = strlen (path);
  while (*filename == '/')
    filename++;
  return xasprintf ("%s%s%s", path,
                    len == 0 || path[len - 1] != '/' ? "/" : "",
                    filename);
}

/* Read a whole file into one NUL-terminated buffer.  maxsz_p, when
 * not NULL, bounds the size and receives it. */
static void *xmalloc_open_read_close (const char *filename, size_t *maxsz_p)
{
  int fd;
  struct stat st;
  size_t size, done;
  char *buf;

  fd = open (filename, O_RDONLY);
  if (fd < 0)
    bb_die ("can't open file");
  if (fstat (fd, &st) < 0)
    {
      close (fd);
      bb_die ("can't stat file");
    }
  size = (size_t) st.st_size;
  if (maxsz_p != NULL && size > *maxsz_p)
    size = *maxsz_p;
  buf = xmalloc (size + 1);
  done = 0;
  while (done < size)
    {
      ssize_t n = read (fd, buf + done, size - done);
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0)
        break;
      done += (size_t) n;
    }
  close (fd);
  buf[done] = '\0';
  if (maxsz_p != NULL)
    *maxsz_p = done;
  return buf;
}

/* The terminal's size, as busybox reads it: TIOCGWINSZ, with
 * LINES/COLUMNS from the environment overriding it and 24x80 the
 * fallback.  Either output pointer may be NULL. */
static unsigned wh_helper (int value, int def_val, const char *envname,
                           int *err)
{
  char *s = getenv (envname);

  if (s != NULL)
    {
      value = atoi (s);
      *err = 0;
    }
  if (value <= 1 || value >= 30000)
    value = def_val;
  return (unsigned) value;
}

static int get_terminal_width_height (int fd, unsigned *width,
                                      unsigned *height)
{
  struct winsize win;
  int err;

  win.ws_row = 0;
  win.ws_col = 0;
  err = ioctl (fd, TIOCGWINSZ, &win) != 0 || win.ws_row == 0;
  if (height != NULL)
    *height = wh_helper (win.ws_row, 24, "LINES", &err);
  if (width != NULL)
    *width = wh_helper (win.ws_col, 80, "COLUMNS", &err);
  return err;
}

/* ------------------------------------------------------------------ */
/* Terminal modes, as busybox's set_termios_to_raw arranges them.      */
/* ------------------------------------------------------------------ */
#define TERMIOS_CLEAR_ISIG      (1 << 0)
#define TERMIOS_RAW_CRNL_INPUT  (1 << 1)
#define TERMIOS_RAW_CRNL_OUTPUT (1 << 2)
#define TERMIOS_RAW_CRNL \
  (TERMIOS_RAW_CRNL_INPUT | TERMIOS_RAW_CRNL_OUTPUT)
#define TERMIOS_RAW_INPUT       (1 << 3)

static int tcsetattr_stdin_TCSANOW (const struct termios *tp)
{
  return tcsetattr (STDIN_FILENO, TCSANOW, tp);
}

static int get_termios_and_make_raw (int fd, struct termios *newterm,
                                     struct termios *oldterm, int flags)
{
  int r;

  memset (oldterm, 0, sizeof (*oldterm));       /* paranoia */
  r = tcgetattr (fd, oldterm);
  *newterm = *oldterm;

  /* unbuffered input, no echo */
  newterm->c_lflag &= ~(ICANON | ECHO | ECHONL);
  if (flags & TERMIOS_CLEAR_ISIG)
    newterm->c_lflag &= ~ISIG;  /* dont recognize INT/QUIT/SUSP */
  /* reads block on a single char */
  newterm->c_cc[VMIN] = 1;
  newterm->c_cc[VTIME] = 0;
  if (flags & TERMIOS_RAW_CRNL_INPUT)
    newterm->c_iflag &= ~(IXON | ICRNL);        /* keep CR, plain XON/XOFF */
  if (flags & TERMIOS_RAW_CRNL_OUTPUT)
    newterm->c_oflag &= ~ONLCR; /* NL stays NL, not CR+NL */
  if (flags & TERMIOS_RAW_INPUT)
    newterm->c_iflag &= ~(IXOFF | IXON | IXANY | BRKINT | INLCR | ICRNL);
  return r;
}

static int set_termios_to_raw (int fd, struct termios *oldterm, int flags)
{
  struct termios newterm;

  get_termios_and_make_raw (fd, &newterm, oldterm, flags);
  return tcsetattr (fd, TCSANOW, &newterm);
}

/* ------------------------------------------------------------------ */
/* Signal-restarted read and poll, and the full_* versions that       */
/* carry the whole buffer through.                                     */
/* ------------------------------------------------------------------ */
static int index_in_strings (const char *strings, const char *key)
{
  int j = -1;

  while (*strings != '\0')
    {
      j++;
      if (strcmp (strings, key) == 0)
        return j;
      strings += strlen (strings) + 1;
    }
  return -1;
}

/* strcpy that tolerates dst and src overlapping. */
static char *overlapping_strcpy (char *dst, const char *src)
{
  while ((*dst = *src) != '\0')
    {
      dst++;
      src++;
    }
  return dst;
}

static ssize_t safe_read (int fd, void *buf, size_t count)
{
  ssize_t n;

  do
    n = read (fd, buf, count);
  while (n < 0 && errno == EINTR);
  return n;
}

static int safe_poll (struct pollfd *ufds, nfds_t nfds, int timeout)
{
  int n;

  do
    n = poll (ufds, nfds, timeout);
  while (n < 0 && errno == EINTR);
  return n;
}

static ssize_t full_read (int fd, void *buf, size_t len)
{
  ssize_t cc;
  ssize_t total = 0;

  while (len != 0)
    {
      cc = safe_read (fd, buf, len);
      if (cc < 0)
        return total > 0 ? total : cc;
      if (cc == 0)
        break;
      buf = (char *) buf + cc;
      total += cc;
      len -= (size_t) cc;
    }
  return total;
}

static ssize_t full_write (int fd, const void *buf, size_t len)
{
  ssize_t cc;
  ssize_t total = 0;

  while (len != 0)
    {
      cc = write (fd, buf, len);
      if (cc < 0)
        {
          if (errno == EINTR)
            continue;
          return total > 0 ? total : cc;
        }
      buf = (const char *) buf + cc;
      total += cc;
      len -= (size_t) cc;
    }
  return total;
}

/* ------------------------------------------------------------------ */
/* linked lists, as getopt32 hands repeatable options to their caller. */
/* ------------------------------------------------------------------ */
static void llist_add_to_end (llist_t **head, char *data)
{
  llist_t *new = xmalloc (sizeof (*new));

  new->data = data;
  new->next = NULL;
  if (*head == NULL)
    *head = new;
  else
    {
      llist_t *tail = *head;
      while (tail->next != NULL)
        tail = tail->next;
      tail->next = new;
    }
}

static void *llist_pop (llist_t **head)
{
  void *data;
  llist_t *next;

  if (*head == NULL)
    return NULL;
  data = (*head)->data;
  next = (*head)->next;
  free (*head);
  *head = next;
  return data;
}

/* ------------------------------------------------------------------ */
/* getopt32: busybox's option parser, here a wrapper around getopt(3) */
/* that keeps the two behaviors vi uses - one char * stored per       */
/* "x:" option, and "x:*" options collected into an llist - and       */
/* returns the same bitmask, one bit per option in optstring order.   */
/* ------------------------------------------------------------------ */
static unsigned getopt32 (char **argv, const char *applet_opts, ...)
{
  char optc[64];
  char gstr[128];
  char *argp[64];
  int takesarg[64];
  int islist[64];
  unsigned bits = 0;
  int nopts = 0, gi = 0, i, argc = 0, c;
  va_list ap;

  while (argv[argc] != NULL)
    argc++;

  /* Split "c:*HhR" into its options, noting which take an argument
   * and which collect a list, then build getopt's "c:HhR". */
  for (i = 0; applet_opts[i] != '\0' && nopts < 64; i++)
    {
      char ch = applet_opts[i];
      if (ch == ':' || ch == '+')
        continue;
      optc[nopts] = ch;
      argp[nopts] = NULL;
      takesarg[nopts] = 0;
      islist[nopts] = 0;
      nopts++;
      gstr[gi++] = ch;
      if (applet_opts[i + 1] == ':')
        {
          gstr[gi++] = ':';
          takesarg[nopts - 1] = 1;
          if (applet_opts[i + 2] == '*')
            {
              islist[nopts - 1] = 1;
              i += 2;           /* repeatable: collected, not stored */
            }
        }
    }
  gstr[gi] = '\0';

  /* The pointers come in the order of the argument-taking options. */
  va_start (ap, applet_opts);
  for (i = 0; i < nopts; i++)
    {
      if (takesarg[i])
        argp[i] = va_arg (ap, char *);
    }
  va_end (ap);

  opterr = 0;
  optind = 0;
  while ((c = getopt (argc, (char *const *) argv, gstr)) != -1)
    {
      for (i = 0; i < nopts; i++)
        {
          if (optc[i] != c)
            continue;
          bits |= 1u << i;
          if (takesarg[i])
            {
              if (islist[i])
                llist_add_to_end ((llist_t **) argp[i], optarg);
              else
                *(char **) argp[i] = optarg;
            }
          break;
        }
      if (i == nopts)
        {
          fprintf (stderr, "vi: invalid option -- '%c'\n", c);
          fprintf (stderr, "Usage: vi [-R] [-c CMD] [FILE]...\n");
          exit (2);
        }
    }
  return bits;
}

/* ------------------------------------------------------------------ */
/* Keycodes: what read_key returns for the escape sequences it knows. */
/* ------------------------------------------------------------------ */
enum
{
  KEYCODE_UP = -2,
  KEYCODE_DOWN = -3,
  KEYCODE_RIGHT = -4,
  KEYCODE_LEFT = -5,
  KEYCODE_HOME = -6,
  KEYCODE_END = -7,
  KEYCODE_INSERT = -8,
  KEYCODE_DELETE = -9,
  KEYCODE_PAGEUP = -10,
  KEYCODE_PAGEDOWN = -11,
  KEYCODE_BACKSPACE = -12,     /* only when Alt/Ctrl/Shifted */
  KEYCODE_D = -13,             /* only when Alted */
  KEYCODE_CTRL_RIGHT = KEYCODE_RIGHT & ~0x40,
  KEYCODE_CTRL_LEFT = KEYCODE_LEFT & ~0x40,
  KEYCODE_ALT_RIGHT = KEYCODE_RIGHT & ~0x20,
  KEYCODE_ALT_LEFT = KEYCODE_LEFT & ~0x20,
  KEYCODE_ALT_BACKSPACE = KEYCODE_BACKSPACE & ~0x20,
  KEYCODE_ALT_D = KEYCODE_D & ~0x20,
  KEYCODE_CURSOR_POS = -0x100,
  KEYCODE_BUFFER_SIZE = 16
};

/* read_key, vendored from busybox libbb: decode one key from fd,
 * returning the byte for plain keys, a KEYCODE_* value for the
 * cursor and editing keys, and -1 on EOF, error, or timeout.
 * buffer[0] counts pending bytes carried between calls. */
static int64_t read_key (int fd, char *buffer, int timeout)
{
  struct pollfd pfd[1];
  const char *seq;
  int n;

  static const char esccmds[] ALIGN1 = {
    '\x7f' | 0x80, KEYCODE_ALT_BACKSPACE,
    '\b' | 0x80, KEYCODE_ALT_BACKSPACE,
    'd' | 0x80, KEYCODE_ALT_D,
    'f' | 0x80, KEYCODE_ALT_RIGHT,
    'b' | 0x80, KEYCODE_ALT_LEFT,
    'O', 'A' | 0x80, KEYCODE_UP,
    'O', 'B' | 0x80, KEYCODE_DOWN,
    'O', 'C' | 0x80, KEYCODE_RIGHT,
    'O', 'D' | 0x80, KEYCODE_LEFT,
    'O', 'H' | 0x80, KEYCODE_HOME,
    'O', 'F' | 0x80, KEYCODE_END,
    '[', 'A' | 0x80, KEYCODE_UP,
    '[', 'B' | 0x80, KEYCODE_DOWN,
    '[', 'C' | 0x80, KEYCODE_RIGHT,
    '[', 'D' | 0x80, KEYCODE_LEFT,
    '[', 'H' | 0x80, KEYCODE_HOME,   /* xterm */
    '[', 'F' | 0x80, KEYCODE_END,    /* xterm */
    '[', '1', '~' | 0x80, KEYCODE_HOME,   /* vt100 */
    '[', '2', '~' | 0x80, KEYCODE_INSERT,
    '[', '3', '~' | 0x80, KEYCODE_DELETE,
    '[', '4', '~' | 0x80, KEYCODE_END,    /* vt100 */
    '[', '5', '~' | 0x80, KEYCODE_PAGEUP,
    '[', '6', '~' | 0x80, KEYCODE_PAGEDOWN,
    '[', '7', '~' | 0x80, KEYCODE_HOME,   /* vt100 */
    '[', '8', '~' | 0x80, KEYCODE_END,    /* vt100 */
    '[', '1', ';', '3', 'C' | 0x80, KEYCODE_ALT_RIGHT,
    '[', '1', ';', '3', 'D' | 0x80, KEYCODE_ALT_LEFT,
    '[', '1', ';', '5', 'C' | 0x80, KEYCODE_CTRL_RIGHT,
    '[', '1', ';', '5', 'D' | 0x80, KEYCODE_CTRL_LEFT,
    0
  };

  pfd->fd = fd;
  pfd->events = POLLIN;

  buffer++;                     /* saved chars counter is in buffer[-1] */

 start_over:
  errno = 0;
  n = (unsigned char) buffer[-1];
  if (n == 0)
    {
      /* No data. Wait for input. */
      if (timeout >= -1)
        {
          n = poll (pfd, 1, timeout);
          if (n < 0 && errno == EINTR)
            return n;
          if (n == 0)
            {
              errno = EAGAIN;
              return -1;
            }
        }
      /* Read one byte only: reading more breaks pasting. */
      n = read (fd, buffer, 1);
      if (n <= 0)
        return -1;
    }

  {
    unsigned char c = buffer[0];
    n--;
    if (n)
      memmove (buffer, buffer + 1, n);
    if (c != 27)
      {
        buffer[-1] = n;
        return c;
      }
  }

  /* Loop through known ESC sequences */
  seq = esccmds;
  while (*seq != '\0')
    {
      int i = 0;

      while (1)
        {
          if (n <= i)
            {
              if (safe_poll (pfd, 1, 50) == 0)
                goto got_all;
              errno = 0;
              if (safe_read (fd, buffer + n, 1) <= 0)
                {
                  if (errno != EAGAIN)
                    {
                      buffer[-1] = 0;
                      return -1;
                    }
                  goto got_all;
                }
              n++;
            }
          if (buffer[i] != (seq[i] & 0x7f))
            {
              seq += i;
              while (!(*seq & 0x80))
                seq++;
              seq += 2;
              break;
            }
          if (seq[i] & 0x80)
            {
              n = 0;
              buffer[-1] = 0;
              return (signed char) seq[i + 1];
            }
          i++;
        }
    }
  /* No matching sequence: keep reading until a timeout, looking for
   * the terminal's cursor position report. */
  while (n < KEYCODE_BUFFER_SIZE - 1)
    {
      if (safe_poll (pfd, 1, 50) == 0)
        break;
      errno = 0;
      if (safe_read (fd, buffer + n, 1) <= 0)
        {
          if (errno != EAGAIN)
            {
              buffer[-1] = 0;
              return -1;
            }
          break;
        }
      n++;
      if (n >= 5 && buffer[0] == '[' && buffer[n - 1] == 'R'
          && isdigit (buffer[1]))
        {
          char *end;
          unsigned long row, col;

          row = strtoul (buffer + 1, &end, 10);
          if (*end != ';' || !isdigit (end[1]))
            continue;
          col = strtoul (end + 1, &end, 10);
          if (*end != 'R')
            continue;
          if (row < 1 || col < 1 || (row | col) > 0x7fff)
            continue;

          buffer[-1] = 0;
          row |= ((unsigned) (-1) << 15);
          col |= (row << 16);
          return ((int64_t) col << 32) | (uint32_t) KEYCODE_CURSOR_POS;
        }
    }
 got_all:

  if (n <= 1)
    {
      /* Alt-x arrives as ESC x: report ESC, x is kept for the next
       * call. */
      buffer[-1] = n;
      return 27;
    }

  /* Unknown sequence: drop it and wait for a new keypress. */
  buffer[-1] = 0;
  goto start_over;
}

static int64_t safe_read_key (int fd, char *buffer, int timeout)
{
  int64_t r;
  do
    r = read_key (fd, buffer, timeout);
  while (r == -1 && errno == EINTR);
  return r;
}

static void __attribute__ ((__noreturn__)) bb_show_usage (void)
{
  fprintf (stderr, "Usage: vi [-R] [-c CMD] [FILE]...\n");
  exit (1);
}

static void __attribute__ ((__noreturn__))
bb_simple_error_msg_and_die (const char *msg)
{
  fprintf (stderr, "vi: %s\n", msg);
  exit (1);
}

#endif /* BB_H */
