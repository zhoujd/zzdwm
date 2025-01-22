/*
 * The routines in this file read and write ASCII files from the
 * disk.  All of the knowledge about files are here.  A better message
 * writing scheme should be used.
 *
 */
#include	<stdio.h>
#include	"ed.h"
#if MSDOS
#include <string.h>
#endif
#if VMS
#include string
#endif
#if V7
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef MMAP
#include <sys/mman.h>
//fixme: remove when mmap
#include <stdlib.h>
#endif
#endif

static FILE *ffp;				/* File pointer, all functions.	*/

/*
 * Open a file for reading.
 */
int ffropen(char *fn)
{
#if V7
	struct stat sb;
	int m;
#endif
	if ((ffp = fopen(fn, "r")) == NULL)
		return FIOFNF;
#if V7
	if (fstat(fileno(ffp), &sb) < 0)
		return FIOERR;
	if ((m = (sb.st_mode & S_IFMT)) != 0
	    && m != S_IFREG
	    && m != S_IFLNK)
		return FIOERR;		/* not a standard file */
#if 0
/*fixme: should check the user and group */
	if (!(sb.st_mode & S_IWUSR))
		return FIORO;		/* readonly */
#endif
#endif
	return FIOSUC;
}

/*
 * Open a file for writing.
 * Return TRUE if all is well, and FALSE on error (cannot create).
 */
int ffwopen(char *fn)
{
#if	VMS
	register int	fd;
	register char *p;

	if ((p = strchr(fn, ';')) != NULL)	/* JFM: if version number */
		*p = '\0';			/* remove it */
	if ((fd=creat(fn, 0666, "rfm=var", "rat=cr")) < 0
	|| (ffp=fdopen(fd, "w")) == NULL) {
#else
#if MSDOS
	char ligne_dsk[NFILEN];
	register char *p;

	strcpy(ligne_dsk, fn);
	p = strchr(ligne_dsk, '.');
	if (p != NULL)
		*p = '\0';
	strcat(ligne_dsk, ".BAK");		/* backup file */
	unlink(ligne_dsk);			/* delete before .. */
	rename(fn, ligne_dsk);			/* .. rename */
#else
#if V7
	char ligne_dsk[NFILEN];
	struct stat sb;

	strcpy(ligne_dsk, fn);
	strcat(ligne_dsk, "~");			/* backup file */
	unlink(ligne_dsk);			/* delete before .. */
	rename(fn, ligne_dsk);			/* .. rename */
#endif
#endif
	if ((ffp=fopen(fn, "w")) == NULL) {
#endif
		mlwrite("Cannot open file for writing");
		return FIOERR;
	}
#if V7
	if (stat(ligne_dsk, &sb) == 0)
		fchmod(fileno(ffp), sb.st_mode);
#endif
#ifdef __TURBOC__
	setvbuf(ffp, NULL, _IOFBF, 512);	/* buffer 512 bytes */
#endif
#ifdef __ZTC__
	setvbuf(ffp, NULL, _IOFBF, 512);	/* buffer 512 bytes */
#endif
	return FIOSUC;
}

#if 0 /*V7*/
/*JFM: Check if a file is writable */
/* used in file.readin() */
int ffiswr(char *file)
{
	return access(file, W_OK) == 0;
}
#endif

/*
 * Close a file.
 * Should look at the status in all systems.
 */
int ffclose(void)
{
#if	V7
	if (fclose(ffp) != FALSE) {
		mlwrite("Error closing file");
		return(FIOERR);
	}
#else
	fclose(ffp);
#endif
	return FIOSUC;
}

/*
 * Write a line to the already opened file.  The "buf" points to the
 * buffer, and the "nbuf" is its length, less the free newline.
 * Return the status.
 * Check only at the newline.
 */
int ffputline(char *buf, int nbuf)
{
#if VMS
	fprintf(ffp, "%.*s\n", nbuf, buf);
#else
	fwrite(buf, 1, nbuf, ffp);
	putc('\n', ffp);
#endif
	if (ferror(ffp) != FALSE) {
		mlwrite("Write I/O error");
		return FIOERR;
	}
	return FIOSUC;
}

#ifdef MMAP
/*
 * Put the file in memory
 * Return status, file content and file size
 */
int ffgetfile(unsigned char **p_file, unsigned long *p_fsize)
{
	size_t fsize;
	unsigned char *file;
//fixme: used when real mmap
//	int fn;

	if (fseek(ffp, 0L, SEEK_END) < 0) {
		mlwrite("File seek error");
		return FIOERR;
	}
	fsize = ftell(ffp);
	*p_fsize = fsize;
	rewind(ffp);

#if 0 // real mmap
	fn = fileno(ffp);
// next step
	file = mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fn, 0);
	if (!file) {
		mlwrite("File read error");
		return FIOERR;
	}
#else
	file = malloc(fsize);
	if (!file) {
		mlwrite("Out of memory");
		return FIOERR;
	}
	if (fread(file, 1, fsize, ffp) != fsize) {
		free(file);
		mlwrite("File read error");
		return FIOERR;
	}
#endif
	*p_file = file;
	return FIOSUC;
}
#else /* !MMAP */
/*
 * Read a line from a file and store the bytes in the supplied buffer.
 * The "nbuf" is the length of the buffer.  Complain about long lines
 * and lines at the end of the file that don't have a newline present.
 * Check for I/O errors too.  Return status.
 */
int ffgetline(char *buf, int nbuf)
{
	register int	c;
	register int	i;

	i = 0;
	if (fgets(buf, nbuf, ffp) != NULL) {
		for (i = 0; i < nbuf - 1; i++) {
			if ((c = buf[i]) == '\n') {
				buf[i] = 0;
				goto lineread;
			}
		}
		mlwrite("File has long line");
		buf[i] = 0;
		return FIOSUC;
	} else {
		c = EOF;
	}
lineread:
	if (c == EOF) {
		if (ferror(ffp) != FALSE) {
			mlwrite("File read error");
			return FIOERR;
		}
		if (i != 0) {
			mlwrite("File has funny line at EOF");
			buf[i] = 0;		/* Make funny line show up. */
			return FIOSUC;
		}
		return FIOEOF;
	}
	buf[i] = 0;
	return FIOSUC;
}
#endif /* MMAP */
