#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wctype.h>

#include "../tags.c"

BUFFER *curbp;
EWINDOW *curwp;
BUFFER *bheadp;

static BUFFER origin_buffer;
static BUFFER target_buffer;
static EWINDOW window;
static char error_message[128];
static int visit_file_status = TRUE;

static LINE *
make_line (const char *text)
{
  size_t length = strlen (text);
  LINE *line = calloc (1, LINEHDR_SIZE + length);

  assert (line != NULL);
  line->l_size = (int) length;
  line->l_used = (int) length;
  memcpy (lgets (line), text, length);
  return line;
}

static void
make_buffer (BUFFER *buffer, const char *name, LINE *first, LINE *second)
{
  LINE *header = calloc (1, LINEHDR_SIZE);

  assert (header != NULL);
  memset (buffer, 0, sizeof (*buffer));
  strcpy (buffer->b_bname, name);
  buffer->b_linep = header;
  lforw (header) = first;
  lback (header) = second;
  lforw (first) = second;
  lback (first) = header;
  lforw (second) = header;
  lback (second) = first;
}

int
ereply (const char *prompt, char *buffer, int size, ...)
{
  (void) prompt;
  (void) size;
  strcpy (buffer, "alpha");
  return TRUE;
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
visit_file (char *filename)
{
  assert (strcmp (filename, "target.c") == 0);
  if (visit_file_status != TRUE)
    return visit_file_status;
  curbp = &target_buffer;
  curwp->w_bufp = curbp;
  curwp->w_dot.p = firstline (curbp);
  curwp->w_dot.o = 0;
  return visit_file_status;
}

int
swbuffer (BUFFER *buffer)
{
  curbp = buffer;
  curwp->w_bufp = buffer;
  return TRUE;
}

int
gotoline (int flag, int line, int key)
{
  (void) flag;
  (void) key;
  assert (line == 1 || line == 2);
  if (line == 1)
    curwp->w_dot.p = firstline (curbp);
  else
    curwp->w_dot.p = lforw (firstline (curbp));
  curwp->w_dot.o = 0;
  return TRUE;
}

int
cisword (wchar_t character)
{
  return iswalnum ((wint_t) character) || character == '_';
}

int
cisalpha (wchar_t character)
{
  return iswalpha ((wint_t) character);
}

int
ffropen (const char *filename)
{
  (void) filename;
  return FIOERR;
}

int
ffgetline (char **buffer, int *length)
{
  (void) buffer;
  (void) length;
  return FIOEOF;
}

int
ffclose (void)
{
  return FIOSUC;
}

int
main (void)
{
  LINE *origin_first = make_line ("alpha origin");
  LINE *origin_second = make_line ("return here");
  LINE *target_first = make_line ("target first");
  LINE *target_second = make_line ("alpha target");
  tagfile *file;

  make_buffer (&origin_buffer, "origin", origin_first, origin_second);
  make_buffer (&target_buffer, "target", target_first, target_second);
  origin_buffer.b_bufp = &target_buffer;
  target_buffer.b_bufp = NULL;
  bheadp = &origin_buffer;

  memset (&window, 0, sizeof (window));
  curwp = &window;
  curbp = &origin_buffer;
  curwp->w_bufp = curbp;
  curwp->w_dot.p = origin_second;
  curwp->w_dot.o = 2;

  file = findtagfile ("target.c");
  assert (file != NULL);
  assert (addtagref ("alpha", file, 1, 0, TRUE) != NULL);
  assert (addtagref ("alpha", file, 2, 0, TRUE) != NULL);
  havetags = TRUE;

  assert (findtag (FALSE, 1, 0) == TRUE);
  assert (curbp == &target_buffer);
  assert (curwp->w_dot.p == target_first);
  assert (tagorigincount == 1);

  assert (findtag (TRUE, 1, 0) == TRUE);
  assert (curbp == &target_buffer);
  assert (curwp->w_dot.p == target_second);
  assert (tagorigincount == 2);

  assert (findtag (TRUE, -1, 0) == TRUE);
  assert (curbp == &target_buffer);
  assert (curwp->w_dot.p == target_first);
  assert (tagorigincount == 3);

  origin_second->l_used = 1;
  assert (poptagmark (FALSE, 1, 0) == TRUE);
  assert (curbp == &target_buffer);
  assert (curwp->w_dot.p == target_second);
  assert (tagorigincount == 2);

  assert (poptagmark (FALSE, 1, 0) == TRUE);
  assert (curbp == &target_buffer);
  assert (curwp->w_dot.p == target_first);
  assert (tagorigincount == 1);

  assert (poptagmark (FALSE, 1, 0) == TRUE);
  assert (curbp == &origin_buffer);
  assert (curwp->w_dot.p == origin_second);
  assert (curwp->w_dot.o == 1);
  assert (tagorigincount == 0);

  assert (poptagmark (FALSE, 1, 0) == FALSE);
  assert (strcmp (error_message, "No tag mark") == 0);

  visit_file_status = FALSE;
  assert (findtag (FALSE, 1, 0) == FALSE);
  assert (tagorigincount == 0);
  visit_file_status = TRUE;

  assert (findtag (FALSE, 1, 0) == TRUE);
  assert (tagorigincount == 1);
  assert (tagorigins[0].buffer == &origin_buffer);
  removetagbuffer (&origin_buffer);
  assert (tagorigincount == 0);
  assert (poptagmark (FALSE, 1, 0) == FALSE);

  free (lback (origin_buffer.b_linep));
  free (firstline (&origin_buffer));
  free (origin_buffer.b_linep);
  free (lback (target_buffer.b_linep));
  free (firstline (&target_buffer));
  free (target_buffer.b_linep);
  freetags (FALSE, 1, 0);
  printf ("tag tests passed\n");
  return 0;
}
