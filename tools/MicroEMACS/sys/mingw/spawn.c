/*
    Copyright (C) 2019 Mark Alexander

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

#include "def.h"
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <io.h>
#include <windows.h>
#include <ctype.h>

/*
 * Global declarations.
 */
char *cspec = NULL;		/* Command string.      */

/*
 * Create a subjob with a copy
 * of the command intrepreter in it. When the
 * command interpreter exits, mark the screen as
 * garbage so that you do a full repaint. Bound
 * to "C-C" and called from "C-Z".
 */
int
spawncli (int f, int n, int k)
{
  ttcolor (CTEXT);		/* Normal color.        */
  ttwindow (0, nrow - 1);	/* Full screen scroll.  */
  ttmove (nrow - 1, 0);		/* Last line.           */
  ttflush ();
  ttclose ();
  if (cspec == NULL)
    {                     /* Try to find it.      */
      cspec = getenv ("SHELL"); /* Prefer Linux shell.  */
      if (cspec == NULL)
        cspec = getenv ("COMSPEC");
      if (cspec == NULL)
        cspec = "/c/WINDOWS/system32/cmd.exe";
    }
  spawnlp (0, cspec, cspec, "", "", NULLPTR);
  ttopen ();
  sgarbf = TRUE;
  return TRUE;
}

/*
 * Open a two-way pipe to the specified program, store the
 * input FILE pointer to *infile, store the output FILE pointer
 * to *outfile, and return TRUE if success.
 */
int
openpipe (const char *program, const char *args[],
	  FILE ** infile, FILE ** outfile)
{
  return FALSE;			/* not implemented yet on Windows */
}

/*
 * Create temp file path
 * Windows safe temporary file creation
 */
int
gettempfile (char *path, int size, const char *prefix)
{
  static char temp_dir[NLINE];

  /* GetTempPathA will natively overwrite this whole array block on every pass */
  GetTempPathA (sizeof(temp_dir), temp_dir);
  GetTempFileNameA (temp_dir, "dir", 0, path);
  return TRUE;
}

/*
 * Translates to POSIX drive paths (/c/dir/file)
 */
static const char *
convertposix (const char *path, char *out_buf, size_t buf_size)
{
  if (!path || !out_buf || buf_size == 0)
    {
      return NULL;
    }
  size_t i = 0;
  size_t j = 0;
  if (path[0] != '\0' && path[1] == ':')
    {
      if (j + 3 > buf_size)
        {
          out_buf[0] = '\0';
          return NULL;
        }
      out_buf[j++] = '/';
      out_buf[j++] = (char)tolower((unsigned char)path[0]);
      i = 2;
    }
  while (path[i] != '\0')
    {
      if (j + 2 > buf_size)
        {
          out_buf[0] = '\0';
          return NULL;
        }
      if (path[i] == '\\')
        {
          out_buf[j++] = '/';
        }
      else
        {
          out_buf[j++] = path[i];
        }
      i++;
    }
  out_buf[j] = '\0';
  return out_buf;
}

/*
 * Run a one-liner in a subjob.
 * When the command returns, wait for a single
 * character to be typed, then mark the screen as
 * garbage so a full repaint is done.
 * Bound to "C-X !".
 */
int
spawncmd (int f, int n, int k)
{
  register int s;
  static char line[NLINE];

  if ((s = ereply ("! ", line, sizeof(line))) != TRUE)
    return (s);

  /* Force repaint */
  eerase ();
  sgarbf = TRUE;

  /* Run the command */
  ttputc ('\n');                /* Already have '\r'    */
  ttcolor (CTEXT);              /* Normal color.        */
  ttwindow (0, nrow - 1);       /* Full screen scroll.  */
  ttmove (nrow - 1, 0);         /* Last line.           */
  ttflush ();
  ttclose ();
  if (system (line) == -1)
    printf ("Failed on system %s\n", line);
  else
    printf ("(End)");
  fflush (stdout);              /* to be sure P.K.      */
  while ((s = ttgetc ()) != EOF && s != '\n' && s != '\r');
  printf ("\n");
  ttopen ();
  ttflush ();
  return TRUE;
}

/*
 * Pipe a one line command into a window
 * Bound to "C-X @"
 */
int
spawnpipe (int f, int n, int k)
{
  register int s;
  register BUFFER *bp;            /* pointer to buffer to zot */
  static char line[NLINE];
  static char tmp[NLINE];         /* Clean storage space for path string */
  char bname[] = "*pipe*";
  static char cmd_buf[NLINE*3];   /* Safe buffer to prevent truncation warnings */

  /* Clear the static arrays cleanly at function entry
   * do NOT clear line[0] here
   */
  tmp[0] = '\0';
  cmd_buf[0] = '\0';

  if ((s = ereply ("Pipe: ", line, sizeof (line))) != TRUE)
    return (s);

  /* Force repaint */
  eerase ();
  sgarbf = TRUE;

  if (gettempfile (tmp, sizeof (tmp), "dir") != TRUE)
    goto end;
  /* Construct the full shell command into the large safe buffer */
  snprintf (cmd_buf, sizeof (cmd_buf), "%s >\"%s\" 2>&1", line, tmp);
  if (system (cmd_buf) == -1)
    {
      printf ("Failed on system %s\n", cmd_buf);
      goto end;
    }
  fflush (stdout);
  /* Read back file contents and populate the target microEMACS buffer */
  if ((bp = bfind (bname, TRUE)) != NULL)
    {
      bclear (bp);
      swbuffer (bp);
      if (readin (tmp) == FALSE)
        goto end;
      strcpy (bp->b_bname, bname);
      strcpy (bp->b_fname, "");
    }

end:
  if (tmp[0] != '\0')
    remove (tmp);
  return (TRUE);
}

/*
 * Filter a buffer through an external program
 * Bound to "C-X #"
 */
int
spawnfilter (int f, int n, int k)
{
  register int s;          /* return status from CLI */
  register BUFFER *bp;     /* pointer to buffer to zot */
  static char line[NLINE];
  char bname[] = "*filter*";
  static char filin[NLINE];       /* Safe storage space for input path string */
  static char filout[NLINE];      /* Safe storage space for output path string */
  static char cmd_buf[NLINE*4];   /* Safe buffer to hold line + input path */

  /* Clear the static arrays cleanly at function entry
   * do NOT clear line[0] here
   */
  filin[0] = '\0';
  filout[0] = '\0';
  cmd_buf[0] = '\0';

  if (curbp->b_flag & BFRO) /* if buffer is read-only       */
    return (FALSE);         /* fail                         */

  if ((s = ereply ("# ", line, sizeof (line))) != TRUE)
    return (s);

  /* Force repaint */
  eerase ();
  sgarbf = TRUE;

  if (gettempfile (filin, sizeof (filin), "me") != TRUE
      || gettempfile (filout, sizeof (filout), "me") != TRUE)
    goto end;
  if (writeout (filin) != TRUE)
    goto end;
  /* Construct the full filter command into the large safe buffer */
  snprintf (cmd_buf, sizeof (cmd_buf), "%s \"%s\" >\"%s\" 2>&1",
            line, filin, filout);
  if (system (cmd_buf) == -1)
    {
      printf ("Failed on system %s\n", cmd_buf);
      goto end;
    }
  fflush (stdout);
  /* Read back filtered contents and populate the target buffer */
  if ((bp = bfind (bname, TRUE)) != NULL)
    {
      bclear (bp);
      swbuffer (bp);
      if (readin (filout) == FALSE)
        goto end;
      strcpy (bp->b_bname, bname);
      strcpy (bp->b_fname, "");
    }

end:
  if (filin[0] != '\0')
    remove (filin);
  if (filout[0] != '\0')
    remove (filout);
  return (TRUE);
}

/*
 * Change current work directory
 * Bound to "C-X $".
 */
int
changedir (int f, int n, int k)
{
  register int s;
  static char line[NLINE];
  static char tmp_path[NLINE];
  char *dname;

  /* Clear the static arrays cleanly at function entry
   * do NOT clear line[0] here
   */
  tmp_path[0] = '\0';

  s = egetdname ("Path: ", line, sizeof (line));
  /* User pressed Enter without typing a path -> display current CWD */
  if (s == FALSE)
    {
      if (getcwd (line, sizeof (line)) == NULL)
        {
          eprintf ("Failed to getcwd.");
          eerase ();
          sgarbf = TRUE;
          return FALSE;
        }
      convertposix (line, tmp_path, NLINE);
      eprintf ("CWD: %s", tmp_path);
      return TRUE;
    }
  else if (s == ABORT)
    {
      return ABORT;
    }
  /* Expand ~ or ~username */
  dname = fftilde (line);
  /*
   * On Windows, SetCurrentDirectoryA changes both the current drive
   * AND directory simultaneously, supporting both '/' and '\'.
   */
  if (!SetCurrentDirectoryA (dname))
    {
      eprintf ("Failed to chdir: %s", dname);
      eerase ();
      sgarbf = TRUE;
      return FALSE;
    }
  /* Fetch and print the canonical absolute directory after change */
  if (getcwd (line, sizeof (line)) != NULL)
    {
      convertposix (line, tmp_path, NLINE);
      eprintf ("CWD: %s", tmp_path);
    }
  else
    eprintf ("CWD: %s", dname);
  return TRUE;
}

/*
 * List current directory
 * Bound to "C-X d".
 */
int
dired (int f, int n, int k)
{
  register int s;
  register BUFFER *bp;
  static char line[NLINE];
  static char buf[NLINE*3];
  static char tmp_path[NLINE];
  char bname[] = "*dired*";

  /* Clear the static arrays cleanly at function entry
   * do NOT clear line[0] here
   */
  buf[0] = '\0';
  tmp_path[0] = '\0';

  s = egetdname ("Dired: ", line, sizeof(line));
  if (s == FALSE)
    snprintf (line, sizeof(line), ".");
  else if (s == ABORT)
    return s;

  /* Force repaint */
  eerase ();
  sgarbf = TRUE;

  gettempfile (tmp_path, NLINE, NULL);
  /* Wrap directory path in quotes to prevent shell breakage on spaces */
  snprintf (buf, sizeof(buf),
            "ls -aBhl --group-directories-first \"%s\" > \"%s\" 2>&1",
            fftilde(line), tmp_path);
  if (system (buf) == 0)
    {
      if ((bp = bfind (bname, TRUE)) != NULL)
        {
          bclear (bp);
          swbuffer (bp);
          readin (tmp_path);
          strcpy (bp->b_bname, bname);
          strcpy (bp->b_fname, "");
        }
    }
  /* Cleanup temp file */
  remove (tmp_path);
  return TRUE;
}
