/* Convert files for Emacs Hexl mode.
   Copyright (C) 1989, 2001-2021 Free Software Foundation, Inc.

   Author: Keith Gabryelski (according to authors.el)

   This file is not considered part of GNU Emacs.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef O_BINARY
# define O_BINARY 0
#endif

static char *progname;

static int
set_binary_mode (int fd, int mode)
{
  return O_BINARY;
}

static _Noreturn void
output_error (void)
{
  fprintf (stderr, "%s: write error\n", progname);
  exit (EXIT_FAILURE);
}

static int
hexchar (int c)
{
  return c - ('0' <= c && c <= '9' ? '0' : 'a' - 10);
}

static void usage()
{
  fprintf (stderr,
           "usage: %s [option] [file]\n"
           "option:\n"
           "-hex  hex dump.\n"
           "-iso  iso character set.\n"
           "-g1 || -group-by-8-bits\n"
           "-g2 || -group-by-16-bits\n"
           "-g4 || -group-by-32-bits\n"
           "-g8 || -group-by-64-bits\n"
           "-un || -de  from hexl format to binary.\n"
           "--  End switch list.\n"
           "<filename>  dump filename.\n"
           "-  (as filename == stdin)\n"
           "-h  print help\n"
           , progname);
}

int
main (int argc, char **argv)
{
  int status = EXIT_SUCCESS;
  int DEFAULT_GROUPING = 0x01;
  int group_by = DEFAULT_GROUPING;
  bool un_flag = false, iso_flag = false;
  progname = *argv++;

  /*
  ** -hex		hex dump
  ** -group-by-8-bits
  ** -group-by-16-bits
  ** -group-by-32-bits
  ** -group-by-64-bits
  ** -iso		iso character set.
  ** -un || -de	from hexl format to binary.
  ** --		End switch list.
  ** <filename>	dump filename
  ** -		(as filename == stdin)
  */

  for (; *argv && *argv[0] == '-' && (*argv)[1]; argv++)
  {
    /* A switch! */
    if (!strcmp (*argv, "--"))
    {
      argv++;
      break;
    }
    else if (!strcmp (*argv, "-un") || !strcmp (*argv, "-de"))
    {
      un_flag = true;
      set_binary_mode (fileno (stdout), O_BINARY);
    }
    else if (!strcmp (*argv, "-hex"))
      /* Hex is the default and is only base supported.  */;
    else if (!strcmp (*argv, "-iso"))
      iso_flag = true;
    else if (!strcmp (*argv, "-group-by-8-bits") || !strcmp (*argv, "-g1"))
      group_by = 0x00;
    else if (!strcmp (*argv, "-group-by-16-bits") || !strcmp (*argv, "-g2"))
      group_by = 0x01;
    else if (!strcmp (*argv, "-group-by-32-bits") || !strcmp (*argv, "-g4"))
      group_by = 0x03;
    else if (!strcmp (*argv, "-group-by-64-bits") || !strcmp (*argv, "-g8"))
      group_by = 0x07;
    else if (!strcmp (*argv, "-h"))
    {
      usage();
      return EXIT_SUCCESS;
    }
    else
    {
      usage();
      return EXIT_FAILURE;
    }
  }

  char const *filename = *argv ? *argv++ : "-";

  do
  {
    FILE *fp;

    if (!strcmp (filename, "-"))
    {
      fp = stdin;
      if (!un_flag)
        set_binary_mode (fileno (stdin), O_BINARY);
    }
    else
    {
      fp = fopen (filename, un_flag ? "r" : "rb");
      if (!fp)
	    {
	      perror (filename);
	      status = EXIT_FAILURE;
	      continue;
	    }
    }

    if (un_flag)
    {
      for (int c; 0 <= (c = getc (fp)); )
	    {
	      /* Skip address at start of line.  */
	      if (c != ' ')
          continue;

	      for (int i = 0; i < 16; i++)
        {
          c = getc (fp);
          if (c < 0 || c == ' ')
            break;

          int hc = hexchar (c);
          c = getc (fp);
          if (c < 0)
            break;
          putchar (hc * 0x10 + hexchar (c));

          if ((i & group_by) == group_by)
          {
            c = getc (fp);
            if (c < 0)
              break;
          }
        }

	      while (0 <= c && c != '\n')
          c = getc (fp);
	      if (c < 0)
          break;
	      if (ferror (stdout))
          output_error ();
	    }
    }
    else
    {
      int c = 0;
      char string[18];
      string[0] = ' ';
      string[17] = '\0';
      for (uintmax_t address = 0; 0 <= c; address += 0x10)
	    {
	      int i;
	      for (i = 0; i < 16; i++)
        {
          if (0 <= c)
            c = getc (fp);
          if (c < 0)
          {
            if (!i)
              break;

            fputs ("  ", stdout);
            string[i + 1] = '\0';
          }
          else
          {
            if (!i)
              printf ("%08"PRIxMAX": ", address);

            string[i + 1]
              = (c < 0x20 || (0x7F <= c && (!iso_flag || c < 0xa0))
                 ? '.' : c);

            printf ("%02x", c + 0u);
          }

          if ((i & group_by) == group_by)
            putchar (' ');
        }

	      if (i)
          puts (string);

	      if (ferror (stdout))
          output_error ();
	    }
    }

    bool trouble = ferror (fp) != 0;
    trouble |= fp != stdin && fclose (fp) != 0;
    if (trouble)
    {
      fprintf (stderr, "%s: read error\n", progname);
      status = EXIT_FAILURE;
    }

    filename = *argv++;
  }
  while (filename);

  if (ferror (stdout) || fclose (stdout) != 0)
    output_error ();
  return status;
}
