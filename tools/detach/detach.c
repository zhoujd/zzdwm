/*
 * detach.c
 *
 * Copyright (C) 2000 Jim Garrison
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	if (argc == 1 || argv[1][0] == '-') {

		if (argc == 2 && argv[1][1] == 'v') {
			fprintf(stderr, PACKAGE " " VERSION " -- "
				"Copyright (C) 2000 Jim Garrison\n"
				PACKAGE " comes with ABSOLUTELY NO WARRANTY.  "
				"This is free software,\nand you are welcome to"
				" redistribute it under certain conditions\n");

		} else {
			fprintf(stderr,
				"Usage: " PACKAGE " program arguments ...\n"
				"       " PACKAGE " -v (shows version number)\n");	
		}

	} else {

		fclose(stdin);
		*stdin = *fopen("/dev/null", "r");

		if (fork())
			return 0;
		setsid();
		if (fork())
			return 0;
		execvp(argv[1], &argv[1]);

	}

	return 0;
}
