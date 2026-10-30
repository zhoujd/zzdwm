/*
 * Command-line help.
 */

#include "def.h"

#include <stdio.h>

#ifdef MINIMAL

#if BACKUP
#define OPT_BACKUP "b"
#else
#define OPT_BACKUP ""
#endif

void
usage (FILE *stream)
{
  fprintf (stream, "usage: me [-234" OPT_BACKUP "DmnNqrTuxz] [-c path] [-d path] [-g line]"
                   " [-p profile] [-s string] [-t size]\n"
                   "          [+[line]] [file[:line[:column]] ...]"
                   " [file://path[:line[:column]] ...]\n");
}
#else

static void
usage_option (FILE *stream, const char *option, const char *description)
{
  fprintf (stream, "%-20s %s\n", option, description);
}

void
usage (FILE *stream)
{
  fputs ("Usage: me [OPTION...] [+[LINE]] [FILE[:LINE[:COLUMN]]...]\n"
         "\n"
         "Options:\n",
         stream);

  fputs ("\nInitialization:\n", stream);
  usage_option (stream, "-d DIRECTORY", "change to directory before opening files");
  usage_option (stream, "-p PROFILE", "load a startup profile");
#ifdef LISP
  usage_option (stream, "-q", "skip default profile and Lisp startup");
#else
  usage_option (stream, "-q", "skip the default profile");
#endif
#ifdef LISP
  usage_option (stream, "-l LISP", "load a Lisp startup file");
#endif
  usage_option (stream, "-r", "open buffers read-only");

  fputs ("\nAutomation:\n", stream);
  usage_option (stream, "-s STRING", "search for string after startup");
#ifdef LISP
  usage_option (stream, "--script SCRIPT", "run a Lisp script without a terminal");
#endif
  usage_option (stream, "--help", "display this help and exit");
  usage_option (stream, "--version", "display version and exit");

  fputs ("\nDisplay:\n", stream);
  usage_option (stream, "-2, -3, -4", "divide the terminal into pages");
  usage_option (stream, "-n", "display line numbers");
#ifdef COLOR
  usage_option (stream, "-C", "disable color output");
#endif
  usage_option (stream, "-m", "enable mouse support (click to move point)");
  usage_option (stream, "-x", "use XON/XOFF flow control");

  fputs ("\nFile handling:\n", stream);
  usage_option (stream, "FILE:LINE:COLUMN", "open a file at a line and column");
  usage_option (stream, "+LINE", "open the first file at a line");
  usage_option (stream, "-g LINE", "go to a line in the first file");
#if BACKUP
  usage_option (stream, "-b", "create backups when saving");
#endif
  usage_option (stream, "-D", "use DOS/CRLF line endings");
  usage_option (stream, "-N", "do not add a final newline");
  usage_option (stream, "-T", "expand tabs into spaces when saving");
  usage_option (stream, "-t SIZE", "set tab size (2-32; default 8)");
  usage_option (stream, "-z", "write a legacy Control-Z EOF marker");

  fputs ("\nTools:\n", stream);
  usage_option (stream, "-c PATH", "set the cscope executable path");
  usage_option (stream, "-u", "do not update the cscope database");
}
#endif
