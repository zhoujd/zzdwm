/*
    Copyright (C) 2008 Mark Alexander

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

/* $Header: /home/bloovis/cvsroot/pe/main.c,v 1.3 2005-10-18 02:18:05 bloovis Exp $
 *
 * Name:	MicroEMACS
 *		Mainline, macro commands.
 * Version:	29
 * By:		rex::conroy
 *		decvax!decwrl!dec-rhea!dec-rex!conroy
 *
 * $Log: main.c,v $
 * Revision 1.3  2005-10-18 02:18:05  bloovis
 * Rename some things to avoid conflict with ncurses.
 *
 * Revision 1.2  2005/05/31 18:18:22  bloovis
 * (bufinit): w_savep was uninitialized; clear it.
 *
 * Revision 1.1.1.1  2003/11/06 02:51:52  bloovis
 * Imported sources
 *
 * Revision 1.7  2002/01/23 22:36:02  malexander
 * (mouse): New variable for mouse support, set by -m option.
 *
 * Revision 1.6  2001/03/05 16:04:06  malexander
 * (bufinit): Clear mark ring of new window.
 *
 * Revision 1.5  2001/02/28 21:07:40  malexander
 * * def.h (POS): New structure for holding line position, which replaces
 * dot and mark variable pairs everywhere.
 *
 * Revision 1.4  2000/11/01 22:00:43  malexander
 * (main): Don't make buffer read-only; bcreate does that now.
 *
 * Revision 1.3  2000/09/29 00:19:38  malexander
 * Numerous changes to eliminate warnings and add prototypes.
 *
 * Revision 1.2  2000/07/21 16:20:32  malexander
 * Reformatted with GNU indent.
 *
 * Revision 1.1.1.1  2000/07/14 19:23:10  malexander
 * Imported sources
 *
 * Revision 1.4  1996/10/22 15:58:57  marka
 * Allow "C-U -" to be treated as a shortcut for -1.
 *
 * Revision 1.3  91/01/07  10:26:04  alexande
 * Remove C++ warnings.  Add tab size variable.
 *
 * Revision 1.2  90/07/03  13:21:05  alexande
 * Changed pat[] to uchar to be consistent with new declaration in def.h.
 *
 *
 */
#include "def.h"

int thisflag;                      /* Flags, this command          */
int lastflag;                      /* Flags, last command          */
int curgoal;                       /* Goal column                  */
BUFFER *curbp = 0;                 /* Current buffer               */
EWINDOW *curwp = 0;                /* Current window               */
BUFFER *bheadp;                    /* BUFFER listhead              */
EWINDOW *wheadp;                   /* EWINDOW listhead             */
BUFFER *blistp;                    /* Buffer list BUFFER           */
int kbdm[NKBDM] = { KCTLX | ')' }; /* Macro                        */
int *kbdmip;                       /* Input  for above             */
int *kbdmop;                       /* Output for above             */
uchar pat[NPAT] = { 0 };           /* Pattern                      */
SYMBOL *symbol[NSHASH];            /* Symbol table listhead.       */
int inprof;                        /* True if reading profile      */
int bflag;                         /* True if -b option specified  */
char *cscope_path = "cscope";      /* Name of cscope program       */
int noupdatecscope;                /* True if -u option specified  */
int mouse;                         /* True if -m option specified  */
int rflag;                         /* True if -r option specified  */
int scriptmode;                    /* True if running a Lisp script */
int xflag;                         /* True if -x option specified  */
int zflag;                         /* True if -z option specified  */
int casefold = TRUE;               /* True if searches fold case   */
int numberflag = FALSE;            /* True if line numbers shown   */
#ifdef COLOR
int colorflag = TRUE;              /* True if colors are enabled   */
int modefg256 = 15;                /* 256-color mode line foreground */
int modebg256 = 22;                /* 256-color mode line background */
int modefg = 0;                    /* Basic mode line foreground    */
int modebg = 2;                    /* Basic mode line background    */
#endif
int fillcol = 70;                  /* Fill column for paragraphs.  */
int tabsize = 8;                   /* No. of columns for a tab     */
int savetabs = TRUE;               /* True if tabs are preserved   */
int autonl = TRUE;                 /* True if auto add newline     */
char *srchstr = NULL;              /* Initial search string passed */
char *targetdir = NULL;            /* Target work directory passed */
int dosmode = FALSE;               /* True if -D forces DOS/CRLF   */

static int nbuf;                   /* number of buffers            */

/*
 * Forward declarations.
 */
static void bufinit (const char *fname);
static int execute (int c, int f, int n);
#ifdef LISP
static int parsescript (int *argcp, char *argv[], char **scriptname);
#endif
static void usage_option (FILE *stream, const char *option,
                          const char *description);
static void usage (FILE *stream);

/*
 * Usage
 */
static void
usage_option (FILE *stream, const char *option, const char *description)
{
  fprintf (stream, "  %-20s %s\n", option, description);
}

static void
usage (FILE *stream)
{
  fputs ("Usage: me [OPTION...] [+[LINE]] [FILE[:LINE[:COLUMN]]...]\n"
         "\n"
         "Options:\n", stream);

  fputs ("\nInitialization:\n", stream);
  usage_option (stream, "-d DIRECTORY",
                "change to directory before opening files");
  usage_option (stream, "-p PROFILE", "load a startup profile");
#ifdef LISP
  usage_option (stream, "-q",
                "skip default profile and Lisp startup");
#else
  usage_option (stream, "-q", "skip the default profile");
#endif
#ifdef LISP
  usage_option (stream, "-l LISP", "load a Lisp startup file");
#endif
  usage_option (stream, "-r", "open buffers read-only");

  fputs ("\nAutomation:\n", stream);
  usage_option (stream, "-s STRING",
                "search for string after startup");
#ifdef LISP
  usage_option (stream, "--script SCRIPT",
                "run a Lisp script without a terminal");
#endif
  usage_option (stream, "--help", "display this help and exit");
  usage_option (stream, "--version", "display version and exit");

  fputs ("\nDisplay:\n", stream);
  usage_option (stream, "-2, -3, -4",
                "divide the terminal into pages");
  usage_option (stream, "-n", "display line numbers");
#ifdef COLOR
  usage_option (stream, "-C", "disable color output");
#endif
  usage_option (stream, "-m", "enable experimental mouse support");
  usage_option (stream, "-x", "use XON/XOFF flow control");

  fputs ("\nFile handling:\n", stream);
  usage_option (stream, "FILE:LINE:COLUMN",
                "open a file at a line and column");
  usage_option (stream, "+LINE", "open the first file at a line");
  usage_option (stream, "-g LINE", "go to a line in the first file");
#if BACKUP
  usage_option (stream, "-b", "create backups when saving");
#endif
  usage_option (stream, "-D", "use DOS/CRLF line endings");
  usage_option (stream, "-N", "do not add a final newline");
  usage_option (stream, "-T", "expand tabs into spaces when saving");
  usage_option (stream, "-t SIZE",
                "set tab size (2-32; default 8)");
  usage_option (stream, "-z", "write a legacy Control-Z EOF marker");

  fputs ("\nTools:\n", stream);
  usage_option (stream, "-c PATH", "set the cscope executable path");
  usage_option (stream, "-u", "do not update the cscope database");
}

#ifdef LISP
/*
 * Remove the --script option and its argument from the command line.  The
 * option is handled before normal option parsing so the script name is not
 * opened as an edit buffer.  Return FALSE if the option is malformed.
 */
static int
parsescript (int *argcp, char *argv[], char **scriptname)
{
  int argc = *argcp;
  int index;

  *scriptname = NULL;
  for (index = 1; index < argc; index++)
    {
      if (strcmp (argv[index], "--script") != 0)
        continue;
      if (*scriptname != NULL || index + 1 >= argc)
        return FALSE;
      *scriptname = argv[index + 1];
      memmove (&argv[index], &argv[index + 2],
               (size_t) (argc - index - 1) * sizeof (argv[0]));
      argc -= 2;
      index--;
    }
  *argcp = argc;
  return TRUE;
}
#endif

/*
 * Main program.
 */
int
main (int argc, char *argv[])
{
  register int c;
  register int f;
  register int n;
  register int mflag;
  register char *arg;
  char *proptr;
#ifdef LISP
  char *lispptr;
  char *scriptname;
  int scriptstatus;
#endif
  int line = 0;
  int gotoflag = FALSE;
  int skipstartup = FALSE;
  int basec;		/* c stripped of meta character */

#ifdef LISP
  if (parsescript (&argc, argv, &scriptname) != TRUE)
    {
      usage (stderr);
      exit (EXIT_FAILURE);
    }
  scriptmode = scriptname != NULL;
#endif

  if (argc == 2)
    {
      if (strcmp (argv[1], "--help") == 0)
        {
          usage (stdout);
          exit (EXIT_SUCCESS);
        }
      if (strcmp (argv[1], "--version") == 0)
        {
          printversion ();
          exit (EXIT_SUCCESS);
        }
    }

  proptr = NULLPTR;		/* profile name         */
#ifdef LISP
  lispptr = NULLPTR;		/* Lisp startup file    */
#endif
  for (n = 1; n < argc; n++)
    {				/* Search for options   */
      arg = argv[n];
      if (arg[0] == '-')
        {
          switch (arg[1])
            {
            case '2':
            case '3':
            case '4':
              npages = arg[1] - '0';
              break;
#if BACKUP
            case 'b':
              bflag = TRUE;
              break;
#endif
#ifdef COLOR
            case 'C':
              colorflag = FALSE;
              break;
#endif
            case 'c':
              n++;
              if (n < argc)
                cscope_path = argv[n];
              break;
            case 'd':
              n++;
              if (n < argc)
                {
                  targetdir = argv[n];
                }
              break;
            case 'D':
              dosmode = TRUE;
              break;
            case 'g':
              n++;
              if (n < argc)
                {
                  gotoflag = TRUE;
                  line = atoi (argv[n]);
                }
              break;
#ifdef LISP
            case 'l':
              n++;
              if (n < argc)
                {
                  lispptr = argv[n];
                  adjustcase (lispptr);
                }
              break;
#endif
            case 'm':
              mouse = TRUE;
              break;
            case 'n':
              numberflag = TRUE;
              break;
            case 'N':
              autonl = FALSE;
              break;
            case 'p':
              n++;
              if (n < argc)
                {
                  proptr = argv[n];
                  adjustcase (proptr);
                }
              break;
            case 'q':
              skipstartup = TRUE;
              break;
            case 'r':
              rflag = TRUE;
              break;
            case 's':
              n++;
              if (n < argc && argv[n][0] != '-')
                srchstr = argv[n];
              break;
            case 'T':
              savetabs = FALSE;
              break;
            case 't':
              n++;
              if (n < argc)
                {
                  tabsize = atoi (argv[n]);
                  if (tabsize < 2 || tabsize > 32)
                    tabsize = 8;
                }
              break;
            case 'u':
              noupdatecscope = TRUE;
              break;
            case 'x':
              xflag = TRUE;
              break;
            case 'z':
              zflag = TRUE;
              break;
            default:	/* unknown switch */
              /* ignore this for now */
              break;
            }
        }
      else if (arg[0] == '+')
        {
          gotoflag = TRUE;
          line = atoi (&arg[1]);
        }
    }

  if (targetdir) /* Target work directory */
    ffchdir (targetdir);
  if (scriptmode)
    {
      nrow = 24;		/* Headless edit context. */
      ncol = 80;
    }
  else
    vtinit ();			/* Virtual terminal.    */
  if ((blistp = bcreate ("*blist*")) == NULL)	/* Special list buffer. */
    abort ();
  keymapinit ();		/* Symbols, bindings.   */
  upmapinit ();			/* Upper case map table */

  for (n = 1; n < argc; n++)
    {				/* Read in files        */
      arg = argv[n];
      if (arg[0] == '-')
        {			/* ignore options       */
          switch (arg[1])
            {
            case 'c':
            case 'd':
            case 'g':
#ifdef LISP
            case 'l':
#endif
            case 'p':
            case 's':
            case 't':
              if (n + 1 < argc)
                {		/* skip name options    */
                  n++;
                }
              break;
            default:
              break;
            }
        }
      else if (arg[0] != '+')
        {			/* it's a filename      */
          int column = 0;
          const char *lp;	/* pointer to line number */

          /* Strip off file://
           */
          if (memcmp (arg, "file://", 7) == 0)
            arg += 7;

          /* If the filename is followed by :N, where N is a decimal number,
           * go to the specified line number in the file.  If the line
           * number is also followed by :N, go to the specified column.
           */
          char * colon = strchr (arg, ':');
          if (colon != NULL && colon[1] >= '0' && colon[1] <= '9')
            {
              gotoflag = TRUE;
              *colon = '\0';
              lp = colon + 1;
              colon = strchr (lp, ':');
              if (colon != NULL && colon[1] >= '0' && colon[1] <= '9')
                {
                  *colon = '\0';
                  column = atoi (colon + 1);
                }
              line = atoi (lp);
            }
          /* Resolve filename and create buffer */
          if (ffcheckname (arg) == TRUE)
            {
              /* File path is valid/resolvable */
              bufinit (arg);
              readin (arg);
              if (gotoflag)		/* goto line specified  */
                {
                  gotoline (TRUE, line, 0);
                  line = 0;
                  if (column != 0)
                    forwchar (TRUE, column - 1, KRANDOM);
                  /* Reset line state after positioning */
                  gotoflag = FALSE;
                  line = 0;
                }
            }
        }
    }

  if (nbuf == 0)
    {				/* no files read in?    */
      bufinit ("main");		/* make an empty buffer */
    }
  else
    {
      curwp = wheadp;		/* reset current window */
      curbp = curwp->w_bufp;	/* reset current buffer */
    }

  lastflag = 0;			/* Fake last flags.     */

  if (scriptmode)
    inprof = enoecho = FALSE;
  else if (skipstartup && proptr == NULLPTR)
    inprof = enoecho = FALSE;
  else
    {
      inprof = enoecho = (ffpopen (proptr) == FIOSUC);
      /* open default profile */
      if (!inprof && proptr != NULLPTR)	/* -p option failed?    */
        eprintf ("Unable to open profile %s", proptr);
      else
        eprintf ("");
    }

#ifdef LISP
  if (scriptmode)
    {
      scriptstatus = TRUE;
      if (lispptr != NULLPTR)
        scriptstatus = lispstartup (lispptr);
      if (scriptstatus == TRUE)
        scriptstatus = lisprun (scriptname);
      exit (scriptstatus == TRUE ? EXIT_SUCCESS : EXIT_FAILURE);
    }
  if (!skipstartup || lispptr != NULLPTR)
    lispstartup (lispptr);
#endif
  update ();

  /* Startup Search Execution */
  if (srchstr != NULL && srchstr[0] != '\0')
    {
      strncpy ((char *)pat, srchstr, NPAT - 1);
      pat[NPAT - 1] = '\0';
      unicodepat ();            /* Convert pat -> upat & compute patlen! */
      gotobob (FALSE, 1, KRANDOM);       /* Move cursor to Beginning Of Buffer first! */
      forwsrch ();
      update ();
    }

loop:
  if (!inprof && !ttstat ())	/* If not in a profile, */
    update ();			/*  fix up the screen.  */
  c = getkey ();
  if (epresf != FALSE)
    {				/* Stuff on echo line?  */
      eerase ();		/* Get rid of echo line */
      if (!inprof && !ttstat ())
        update ();
    }
  f = FALSE;
  n = 1;

  /* do META-# processing if needed */
  basec = c & ~KMETA;              /* strip meta char off if there */
  if ((c & KMETA) && ((basec >= '0' && basec <= '9') || basec == '-'))
    {
      f = TRUE;               /* there is a # arg */
      n = 0;                  /* start with a zero default */
      mflag = 1;              /* current minus flag */
      c = basec;              /* strip the META */
      while ((c >= '0' && c <= '9') || (c == '-') || (c == (KCTRL | 'G')))
        {
          if (c == '-')
            {
              /* already hit a minus or digit? */
              if ((mflag == -1) || (n != 0))
                break;
              mflag = -1;
            }
          else if (c == (KCTRL | 'G'))
            {
              eprintf ("[Aborted]");
              break;
            }
          else
            {
              n = n * 10 + (c - '0');
            }
          if ((n == 0) && (mflag == -1))  /* lonely - */
            eprintf ("Arg: -");
          else
            eprintf ("Arg: %d", n * mflag);

          c = getkey ();   /* get the next key */
        }
      n = n * mflag;  /* figure in the sign */
    }

  /* do ^U repeat argument processing */
  if (c == (KCTRL | 'U'))
    {				/* ^U, start argument.  */
      f = TRUE;
      n = 4;		/* with argument of 4 */
      mflag = 0;	/* that can be discarded. */
      while (((c = getkey ()) >= '0' && c <= '9') || c == (KCTRL | 'U')
            || c == '-' || c == (KCTRL | 'G'))
        {
          if (c == (KCTRL | 'U'))
            {
              if ((n > 0) == ((n * 4) > 0))
                n = n * 4;
              else
                n = 1;
            }
          else if (c == (KCTRL | 'G'))
            {
              eprintf ("[Aborted]");
              break;
            }
          /*
          * If dash, and start of argument string, set arg.
          * to -1.  Otherwise, insert it.
          */
          else if (c == '-')
            {
              if (mflag)
                break;
              n = 0;
              mflag = -1;
            }
          /*
          * If first digit entered, replace previous argument
          * with digit and set sign.  Otherwise, append to arg.
          */
          else
            {
              if (!mflag)
                {
                  n = 0;
                  mflag = 1;
                }
              n = 10 * n + c - '0';
            }
          if ((n == 0) && (mflag == -1))  /* lonely - */
            eprintf ("Arg: -");
          else
            eprintf ("Arg: %d", (mflag >= 0) ? n : -n);
        }
      /*
      * Make arguments preceded by a minus sign negative and change
      * the special argument "^U -" to an effective "^U -1".
      */
      if (mflag == -1)
        {
          if (n == 0)
            n++;
          n = -n;
        }
    }
  if (kbdmip != NULL)
    {				/* Save macro strokes.  */
      if (c != (KCTLX | ')') && kbdmip > &kbdm[NKBDM - 6])
        {
          ctrlg (FALSE, 0, KRANDOM);
          goto loop;
        }
      if (f != FALSE)
        {
          *kbdmip++ = (KCTRL | 'U');
          *kbdmip++ = n;
        }
      *kbdmip++ = c;
    }
  startsaveundo ();
  execute (c, f, n);		/* Do it.               */
  endsaveundo ();
  replyq_clear ();
  goto loop;
}

/*
 * Command execution. Look up the binding in the the
 * binding array, and do what it says. Return a very bad status
 * if there is no binding, or if the symbol has a type that
 * is not usable (there is no way to get this into a symbol table
 * entry now). Also fiddle with the flags.
 */
static int
execute (int c, int f, int n)
{
  register SYMBOL *sp;
  register int status;

  /* If there is no binding for the key, assume it is
   * a Unicode character that should be self-inserted.
   */
  if ((sp = getbinding (c)) == NULL)
    if (c >= 0x80 && c <= 0x10ffff)
      sp = getbinding (' ');
  if (sp != NULL)
    {
      thisflag = 0;
      startsaveundo ();
      if (sp->s_macro)
        status = domacro (sp->s_macro, n);
      else
        status = (*sp->s_funcp) (f, n, c);
      lastflag = thisflag;
      endsaveundo ();
      return (status);
    }
  eprintf ("Unknown command");
  lastflag = 0;
  return (ABORT);
}

/*
 * Initialize a new buffer and window.  The filename is passed down as
 * an argument, because the main routine may have been told to read in
 * a file by default, and we want the buffer name to be right.  If there
 * is a conflict in buffer names, keep modifying the name until the
 * conflict goes away.  This function also creates up to two initial
 * windows for the first one or two files being edited.
 */
static void
bufinit (const char *fname)
{
  char bname[NBUFN];		/* Buffer name          */
  char *mod;			/* Ptr to name modifier */
  register BUFFER *bp;
  register EWINDOW *wp;
  LINE *lp;

  makename (bname, fname);	/* Get buffer name      */
  unqname (bname, NBUFN);
  if (bfind (bname, FALSE))	/* if names conflict    */
    {
      strcat (bname, ".0");	/* Append modifier      */
      mod = &bname[strlen (bname) - 1];	/* Point to '0' at end  */
      while (bfind (bname, FALSE))	/* While still conflicts */
        (*mod)++;		/* Bump the modifier    */
    }
  if ((bp = bfind (bname, TRUE)) == NULL)	/* Create text buffer.  */
    {
      eprintf ("Out of memory: cannot create buffer");
      if (wheadp != NULL)
        curwp = wheadp;
      return;
    }
  if (scriptmode && nbuf > 0)
    {				/* Reuse headless window. */
      ++nbuf;
      swbuffer (bp);
      return;
    }
  curbp = bp;			/* Current buffer       */
  if (++nbuf <= 2)		/* If 2 or less buffers */
    {				/* Get a new window     */
      if ((wp = (EWINDOW *) malloc (sizeof (EWINDOW))) == NULL)
        {
          eprintf ("Out of memory: cannot create window");
          if (wheadp != NULL)
            curwp = wheadp;
          return;
        }
      curwp = wp;		/* Current window       */
      if (nbuf == 1)		/* First window?        */
        {
          wheadp = wp;
          wp->w_toprow = 0;
        }
      else			/* Second window.   */
        {
          wheadp->w_wndp = wp;
          wp->w_toprow = nrow / 2;	/* 2nd half of screen   */
          wheadp->w_ntrows = wp->w_toprow - 1;	/* shrink  */
        }			/* 1 = mode line        */
      wp->w_ntrows = nrow - wp->w_toprow - 2;	/* 2 = mode,echo */
      wp->w_wndp = NULL;	/* Initialize window.   */
      wp->w_bufp = bp;
      bp->b_nwnd = 1;		/* Displayed.           */
      lp = firstline (bp);
      wp->w_linep = lp;
      wp->w_dot.p = lp;
      wp->w_dot.o = 0;
      clearmarks (&wp->w_ring);
      wp->w_force = 0;
      wp->w_flag = WFMODE | WFHARD;	/* Full.                */
      wp->w_leftcol = 0;	/* Display at left edge */
      wp->w_lwidth = 0;
      wp->w_topline = 0;
      wp->w_savep = NULL;
    }
}

/*
 * Fancy quit command, as implemented
 * by Jeff. If the current buffer has changed
 * do a write current buffer. Otherwise run a command
 * interpreter in a subjob. Two of these will get you
 * out. Bound to "C-Z".
 */
int
jeffexit (int f, int n, int k)
{
  if ((curbp->b_flag & BFCHG) != 0     /* Changed.             */
      && strcmp ("main", curbp->b_bname) != 0)
    return (filesave (f, n, KRANDOM));
  return (spawncli (f, n, KRANDOM));   /* Suspend.             */
}

/*
 * Quit command. If an argument, always
 * quit. Otherwise confirm if a buffer has been
 * changed and not written out. Normally bound
 * to "C-X C-C".
 */
int
quit (int f, int n, int k)
{
  register int s;

  if (f != FALSE		/* Argument forces it.  */
      || anycb () == FALSE	/* All buffers clean.   */
      || (s = eyesno ("Quit")) == TRUE)
    {				/* User says it's OK.   */
      vttidy ();
      exit (GOOD);
    }
  return (s);
}

/*
 * Begin a keyboard macro.
 * Error if not at the top level
 * in keyboard processing. Set up
 * variables and return.
 */
int
ctlxlp (int f, int n, int k)
{
  if (kbdmip != NULL || kbdmop != NULL)
    {
      eprintf ("Not now");
      return (FALSE);
    }
  eprintf ("[Start macro]");
  kbdmip = &kbdm[0];
  return (TRUE);
}

/*
 * End keyboard macro. Check for
 * the same limit conditions as the
 * above routine. Set up the variables
 * and return to the caller.
 */
int
ctlxrp (int f, int n, int k)
{
  if (kbdmip == NULL)
    {
      eprintf ("Not now");
      return (FALSE);
    }
  eprintf ("[End macro]");
  *(kbdmip - 1) = KCTLX | ')';
  kbdmip = NULL;
  return (TRUE);
}

/*
 * Execute the current (un-named) macro.
 * The command argument is the
 * number of times to loop. Quit as
 * soon as a command gets an error.
 * Return TRUE if all ok, else
 * FALSE.
 */
int
ctlxe (int f, int n, int k)
{
  if (f == FALSE)
    {
      n = reglines();
      if (n <= 0)
        n = 1;
    }
  return domacro (&kbdm[0], n);
}

/*
 * Execute the specified macro (either the current
 * keyboard macro, or a named macro).
 * The command argument is the
 * number of times to loop. Quit as
 * soon as a command gets an error.
 * Return TRUE if all ok, else
 * FALSE.
 */
int
domacro (int *macrop, int n)
{
  register int c;
  register int af;
  register int an;
  register int s;

  if (kbdmip != NULL || kbdmop != NULL)
    {
      eprintf ("Not now");
      return (FALSE);
    }
  if (n <= 0)
    return (TRUE);
  do
    {
      kbdmop = macrop;
      do
        {
          af = FALSE;
          an = 1;
          if ((c = *kbdmop++) == (KCTRL | 'U'))
            {
              af = TRUE;
              an = *kbdmop++;
              c = *kbdmop++;
            }
          s = TRUE;
        }
      while (c != (KCTLX | ')') && (s = execute (c, af, an)) == TRUE);
      kbdmop = NULL;
    }
  while (s == TRUE && --n);
  return (s);
}

/*
 * Abort.
 * Beep the beeper.
 * Kill off any keyboard macro,
 * etc., that is in progress.
 * Sometimes called as a routine,
 * to do general aborting of
 * stuff.
 */
int
ctrlg (int f, int n, int k)
{
  ttbeep ();
  if (kbdmip != NULL)
    {
      kbdm[0] = (KCTLX | ')');
      kbdmip = NULL;
    }
  return (ABORT);
}

/*
 * Display the version. All this does
 * is copy the text in the external "version" array into
 * the message system, and call the message reading code.
 * Don't call display if there is an argument.
 */
int
showversion (int f, int n, int k)
{
  register char **cpp;
  register char *cp;

  cpp = &version[0];
  while ((cp = *cpp++) != NULL)
    {
      if (writemsg (cp) == FALSE)
        return (FALSE);
    }
  if (f != FALSE)		/* No display if arg.   */
    return (TRUE);
  return (readmsg ());
}

/*
 * Display the message lines.  At present, only the display-version
 * command uses the message system, but other commands
 * may use it in the future.
 */
int
displaymessage (int f, int n, int k)
{
  return (readmsg ());
}

/*
 * Fancy quit command, as implemented by Norm. If the any buffer has
 * changed do a write on that buffer and exit emacs, otherwise simply exit.
 */
int
quickexit (int f, int n, int k)
{
  BUFFER *bp;	/* scanning pointer to buffers */
  BUFFER *oldcb;	/* original current buffer */
  int status;

  oldcb = curbp;		/* save in case we fail */
  bp = bheadp;
  while (bp != NULL)
    {
      if ((bp->b_flag & BFCHG) != 0)	/* Changed.             */
        {
          curbp = bp;	/* make that buffer cur */
          if (strlen(bp->b_fname) != 0)
            {
              eprintf("(Saving %s)", bp->b_fname);
              if ((status = filesave(f, n, k)) != TRUE)
                {
                  curbp = oldcb;	/* restore curbp */
                  return status;
                }
            }
        }
      bp = bp->b_bufp;	/* on to the next buffer */
    }
  quit(f, n, k);		/* conditionally quit   */
  return TRUE;
}
