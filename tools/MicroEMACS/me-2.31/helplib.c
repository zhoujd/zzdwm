/*
 * This is a library module for help file subject lookup.  There are
 * three externally available functions and two externally available
 * pointers.
 *
 * The two externally available pointers must be defined and given a
 * value by the program that calls this module, and are
 *	char *helpfile;		The name of the help file
 *	char *helpindex;	The name of the help index file
 *
 * The functions that are available are:
 *
 *	char *name_gen(basename, pathvar, defpath) char *basename, *pathvar;
 *		Returns the full name (including path info) of the
 *		file "basename" looked up on pathvar, if found or
 *		defpath.  The name is returned in a malloc'ed chunk
 *		of memory which should be free'd when you are done
 *		with it.  Returns NULL on failure.
 *
 *	int help(topic, output) char *topic; int (*output)();
 *		Lookup the topic and call output with each line of it.
 *		Returns 1 if the topic could not be found, -1 if the
 *		file could not be accessed.
 *
 *	int helpclose();
 *		Close help files and clean up the rest of the world.
 *
 */
#include <stdio.h>
#include <stat.h>
#include <path.h>

#define	NLINE	512			/* Longest helpfile line */
#define PATHSIZE 64			/* Longest path name	*/

#ifdef	GEMDOS
#define	DEFHELPATH	DEFLIBPATH
#endif

#ifdef	MSDOS
#define	DEFHELPATH	DEFLIBPATH
#endif

#ifdef	COHERENT
#define	DEFHELPATH	"/etc:/lib:/usr/lib:"
#endif

#ifndef	HELPSEP
#define	HELPSEP	'@'			/* marks a new help entry */
#endif

extern char	*helpfile;		/* Help file name	*/
extern char	*helpindex;		/* Help file index	*/

static	FILE	*helpfp;		/* Our helpfile		*/
static	char	*helpline;		/* Our help buffer	*/

/*
 * Structure to speed lookup time.
 */
struct	look	{
	long	l_seek;
	char	*l_name;
};

struct	look	*lread();

char	*getenv();
char	*path();

/*
 * Create a file name from a basename and an environment variable.
 */
char *name_gen(name, pathvar, defpath)
register char *name;
char	*pathvar;
char	*defpath;
{
	register char *fullname;
	register char *libpath;

	if ((fullname = (char *)malloc(PATHSIZE)) == NULL)
		return NULL;
	if ((libpath = getenv(pathvar)) == NULL)
		libpath = defpath;
	if ((libpath = path(libpath, name, AREAD)) == NULL)
		strcpy(fullname, name);
	else
		strcpy(fullname, libpath);
	return fullname;
}

/* Open the help file "name" with access "acs" along the current
 * libpath.
 */
static FILE *
hfopen(name, acs)
char *name;
char *acs;
{
	char *fullname;
	FILE *fp = NULL;

	if ((fullname = name_gen(name, "LIBPATH", DEFHELPATH)) == NULL)
		return NULL;		/* Can't get full name	*/
	fp = fopen(fullname, acs);	/* Open the file.	*/
	free(fullname);			/* Free the namebuffer	*/
	return fp;			/* return the file ptr	*/
}

/*
 * Get the "stat" of the help file...
 */
static hstat(name, sb)
char *name;
struct stat *sb;
{
	char *fullname;
	register int s;

	if ((fullname = name_gen(name, "LIBPATH", DEFHELPATH)) == NULL)
		return -1;	/* could not get the name	*/
	s = stat(fullname, sb);	/* stat the file.		*/
	free(fullname);
	return s;
}

helpclose() {
	if (helpfp != NULL)
		fclose(helpfp);
	if (helpline != NULL)
		free(helpline);
	helpfp = helpline = NULL;
}

/*
 * program interface to the help file. open file(s) as needed.
 * subsequent calls will not need to search and open them
 * again -- output function takes a string arg and does something
 * with it.  Non-zero return means no help found.
 */
help(topic, output)
char *topic;
int (*output)();
{
	if (helpfp == NULL)
		if ((helpfp = hfopen(helpfile, "r")) == NULL)
			return -1;
	if (helpline == NULL)
		if ((helpline = malloc(NLINE)) == NULL)
			return -1;
	return(lookup(topic, helpfp, helpindex, output));
}

/*
 * Make an index of help for a given topic.
 * Non-zero return means no topics found.
 */
indexhelp(topic, output)
char *topic;
int (*output)();
{
	if (helpfp == NULL)
		if ((helpfp = hfopen(helpfile, "r")) == NULL)
			return -1;
	if (helpline == NULL)
		if ((helpline = malloc(NLINE)) == NULL)
			return -1;
	return(doindex(topic, helpfp, output));
}

/*
 * Lookup a command in the given file.
 * The format is to look for HELPSEP (@) lines.
 * The optional index-file is provided to speed up
 * the lookup in situations where there is a very
 * large system help file.
 */
static lookup(com, fp, ind, output)
register char *com;
FILE *fp;
char *ind;
int (*output)();
{
	if (fp == NULL)
		return (1);
	fseek(fp, 0L, 0);
	fastlook(com, fp, ind);
	while (fgets(helpline, NLINE, fp) != NULL)
		if (helpline[0] == HELPSEP) {
			helpline[strlen(helpline)-1] = '\0';
			if (strcmp(com, helpline+1) == 0) {
				while (fgets(helpline, NLINE, fp) != NULL) {
					if (helpline[0] == HELPSEP)
						break;
					helpline[strlen(helpline)-1] = '\0';
					(*output)(helpline);
				}
				return (0);
			}
		}
	return (1);
}

/*
 * Possibly seek the helpfile to the right place based on an index file.
 * Return non-zero only when it is impossible to find it.
 */
static fastlook(com, fp, ind)
char *com;
FILE *fp;
char *ind;
{
	register struct look *lp;
	FILE *ifp;
	static struct stat sb;
#if !MSDOS
	long htime;

	fstat(fileno(fp), &sb);
	htime = sb.st_mtime;
#endif
	if (ind == NULL)			/* No index file?	*/
		return;
	if (hstat(ind, &sb) < 0)		/* not found ? */
		return;
#if !MSDOS
	if (htime < sb.st_mtime) {
#endif
		if ((ifp = hfopen(ind, "rb")) == NULL)
			return;
		while ((lp = lread(ifp)) != NULL)
			if (strcmp(com, lp->l_name) == 0) {
				fseek(fp, lp->l_seek, 0);
				break;
			}
		fclose(ifp);
#if !MSDOS
	}
#endif
	return;
}

/*
 * Read in a look structure.  Return NULL
 * on end of file or error.
 */
static struct look *
lread(fp)
register FILE *fp;
{
	register char *cp;
	register int c;
	static struct look look;
	static char name[50];

	look.l_name = name;
	if (fread(&look.l_seek, sizeof look.l_seek, 1, fp) != 1)
		return (NULL);
	for (cp = name; cp<&name[49]; cp++) {
		if ((c = getc(fp)) == EOF)
			return (NULL);
		*cp = c;
		if (c == '\0')
			break;
	}
	return (&look);
}

/* Return 0 if s1 found in s2 */
static subcmp(s1, s2)
char *s1;
register char *s2;
{
	register char *st;
	register char *se;

	st = s1;
	for (;;) {
		while (*st != *s2++)
			if (*s2 == '\0')
				return 1;
		if (*st++ == '\0')
			return 0;
		se = s2;
		while (*st++ == *se++) {
			if (*st == '\0')
				return 0;
			if (*se == '\0')
				return 1;
		}
		st = s1;
	}
}

static int doindex(com, fp, output)
register char *com;
FILE *fp;
int (*output)();
{
	register int t = 0;

	if (fp == NULL)
		return (1);
	fseek(fp, 0L, 0);
	while (fgets(helpline, NLINE, fp) != NULL) {
		if (helpline[0] == HELPSEP) {
			helpline[strlen(helpline)-1] = '\0';
			if (subcmp(com, helpline+1) == 0) {
				if (fgets(helpline, NLINE, fp) == NULL)
					return t == 0;
				helpline[strlen(helpline)-1] = '\0';
				(*output)(helpline);
				t++;
			}
		}
	}
	if (t == 0)
		return 1;
	return 0;
}

