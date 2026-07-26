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

#define TMPBUF_SIZE MAX_PATH

/* extern char *getenv(char *); */

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
  return (TRUE);
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

  (void)size;
  (void)prefix;
  GetTempPathA (sizeof(temp_dir), temp_dir);
  GetTempFileNameA (temp_dir, "dir", 0, path);
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
  static char buf[NLINE * 3];
  char tmp_path[NLINE];
  char bname[] = "*dired*";

  s = egetdname ("Dired: ", line, sizeof(line));
  if (s == FALSE || line[0] == '\0')
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
