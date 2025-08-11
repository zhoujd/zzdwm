/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

void
die(const char *fmt, ...)
{
	va_list ap;
	int saved_errno;

	saved_errno = errno;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':')
		fprintf(stderr, " %s", strerror(saved_errno));
	fputc('\n', stderr);

	exit(1);
}

void *
ecalloc(size_t nmemb, size_t size)
{
	void *p;

	if (!(p = calloc(nmemb, size)))
		die("calloc:");
	return p;
}

void
utf8trunc(char *s, size_t bytes)
{
	unsigned int i;
	size_t pos, len;

	if (s == NULL || bytes == 0) {
		return;
	}

	pos = bytes;
	len = strlen(s);
	if (len <= bytes) {
		return; /* No truncation needed */
	}

	/* Find a valid UTF-8 character boundary */
	while (pos > 0 && (s[pos] & 0xC0) == 0x80) { /* If it's a continuation byte */
		pos--; /* Move back */
	}

	/* Null-terminate at the valid boundary */
	s[pos] = '\0';

	/* Add ... before Null-terminate */
	for (i = 1; i <= 3; i++)
		s[pos - i] = '.';
}
