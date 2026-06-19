/*
 * Filename completion.c
 */
#include "def.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <io.h>
#include <windows.h>

/*
 * Forward declarations.
 */
static void outstring (char *s);

/*
 * basic filename completion, based on code in uemacs/PK
 */
int getfilename (char *prompt, char *buf, int nbuf)
{
  int cpos = 0; /* current character position in string */
  int c;
  int ocpos, nskip = 0, didtry = 0;
  int eolchar = '\n';
  FILE *tmpf = NULL;
  static char tmp[255];

  /* prompt the user for the input string */
  eprintf (prompt);

  for (;;)
    {
      if (!didtry)
        nskip = -1;
      didtry = 0;

      /* get a character from the user */
      c = ttgetc ();

      /* If it is a <ret>, change it to a <NL> */
      if (c == CCHR ('M'))
        c = '\n';

      /* if they hit the line terminate, wrap it up */
      if (c == eolchar)
        {
          buf[cpos++] = 0;

          /* clear the message line */
          eerase ();
          ttflush ();

          /* if we default the buffer, return FALSE */
          if (buf[0] == 0)
            {
              if (tmpf != NULL) { fclose(tmpf); _unlink(tmp); }
              return FALSE;
            }

          if (tmpf != NULL) { fclose(tmpf); _unlink(tmp); }
          return TRUE;
        }

      if (c == CCHR ('G'))
        {
          /* Abort the input? */
          ctrlg (FALSE, 0, KRANDOM);
          ttputc (c);
          ttflush ();
          if (tmpf != NULL) { fclose(tmpf); _unlink(tmp); }
          return ABORT;
        }
      else if ((c == 0x7F || c == 0x08 || c == 0x107))
        {
          /* rubout/erase */
          if (cpos != 0)
            {
              outstring ("\b \b");
              --ttcol;
              if (buf[--cpos] < 0x20)
                {
                  outstring ("\b \b");
                  --ttcol;
                }
              if (buf[cpos] == '\n')
                {
                  outstring ("\b\b  \b\b");
                  ttcol -= 2;
                }
              ttflush ();
            }
        }
      else if (c == 0x15)
        {
          /* C-U, kill */
          while (cpos != 0)
            {
              outstring ("\b \b");
              --ttcol;

              if (buf[--cpos] < 0x20)
                {
                  outstring ("\b \b");
                  --ttcol;
                }
              if (buf[cpos] == '\n')
                {
                  outstring ("\b\b  \b\b");
                  ttcol -= 2;
                }
            }
          ttflush ();
        }
      else if ((c == 0x09 || c == ' ' || c == '?'))
        {
          /* TAB, complete file name */
          static char ffbuf[1024]; /* Expanded to safely hold the bash shell payload */
          int n, iswild = 0;

          didtry = 1;
          ocpos = cpos;
          while (cpos != 0)
            {
              outstring ("\b \b");
              --ttcol;

              if (buf[--cpos] < 0x20)
                {
                  outstring ("\b \b");
                  --ttcol;
                }
              if (buf[cpos] == '\n')
                {
                  outstring ("\b\b  \b\b");
                  ttcol -= 2;
                }
              if (buf[cpos] == '*' || buf[cpos] == '?')
                iswild = 1;
            }
          ttflush ();
          if (nskip < 0)
            {
              buf[ocpos] = 0;
              if (tmpf != NULL)
                {
                  fclose (tmpf);
                  _unlink(tmp);
                  tmpf = NULL;
                }

              /* FIX: Fetch the actual user Temp directory instead of writing to C:\ */
              char win_temp_dir[MAX_PATH];
              GetTempPathA(MAX_PATH, win_temp_dir);
              GetTempFileNameA(win_temp_dir, "me", 0, tmp);

              /* Construct bash command string execution */
              /* Wrapping the inner shell command ensures wildcards evaluate in bash, not Windows */
              strcpy (ffbuf, "bash.exe -c \"echo ");
              strcat (ffbuf, buf);
              if (!iswild)
                strcat (ffbuf, "*");
              strcat (ffbuf, "\" > ");
              strcat (ffbuf, tmp);
              strcat (ffbuf, " 2>&1");

              system (ffbuf);
              tmpf = fopen (tmp, "r");
              nskip = 0;
            }
          c = ' ';
          for (n = nskip; n > 0; n--)
            while ((c = getc (tmpf)) != EOF && c != ' ')
              ;
          nskip++;

          if (c == EOF)
            {
              ttbeep ();
              nskip = 0;
            }
          while ((c = getc (tmpf)) != EOF && c != '\n' && c != ' ' && c != '*')
            {
              if (cpos < nbuf - 1)
                buf[cpos++] = c;
            }
          if (c == '*')
            ttbeep ();

          for (n = 0; n < cpos; n++)
            {
              c = buf[n];
              if ((c < ' ') && (c != '\n'))
                {
                  outstring ("^");
                  ++ttcol;
                  c ^= 0x40;
                }

              if (c != '\n')
                {
                  ttputc (c);
                }
              else
                { /* put out <NL> for <ret> */
                  outstring ("<NL>");
                  ttcol += 3;
                }
              ++ttcol;
            }
          ttflush ();
          rewind (tmpf);
        }
      else
        {
          if (cpos < nbuf - 1)
            {
              /* if a control char */
              if ((c < ' ') && (c != '\n'))
                {
                  ttbeep ();
                }
              else
                {
                  buf[cpos++] = c;
                  ttputc (c);
                  ++ttcol;
                  ttflush ();
                }
            }
        }
    }
}

/*
 * output a string of characters
 * char *s;   string to output
 */
void
outstring (char *s)
{
  while (*s)
    ttputc (*s++);
}
