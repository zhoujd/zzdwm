/*
 * repeat after me, a simple typing exercise
 *
 * Copyright (C) 2008-2012 Ali Gholami Rudi
 *
 * This program is released under GNU GPL version 2.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#define LINELEN			70
#define BUFSZ			128
#define MIN(a, b)		((a) < (b) ? (a) : (b))

static int randvowel(void)
{
	static char vowels[] = "aeiou";
	return vowels[rand() % (sizeof(vowels) - 1)];
}

static int randcons(void)
{
	static char cons[] = "bcdfghjklmnpqrstvwxyz";
	return cons[rand() % (sizeof(cons) - 1)];
}

static int randsymbol(void)
{
	static char symbols[] = "'\";:,<.>/?|\\[{]}`~!@#$%^&*()_-+=";
	return symbols[rand() % (sizeof(symbols) - 1)];
}

static char *genword(char *buf, int size)
{
	char *c = buf;
	int n = MIN(rand() % 10 + 1, size);
	int vowel = rand() % 2;
	int allcaps = !(rand() % 20);

	while (n--) {
		if (rand() % (!n ? 25 : 50)) {
			if (vowel)
				if (rand() % 30)
					*c = randvowel();
				else
					*c = randcons();
			else if (rand() % 50)
				*c = randcons();
			else
				*c = randvowel();
			if (allcaps || (c == buf && !(rand() % 7)))
				*c = toupper(*c);
		} else {
			*c = randsymbol();
		}
		vowel = 1 - vowel;
		c++;
	}
	return c;
}

static char *gennumber(char *buf, int size)
{
	char *c = buf;
	int n = MIN(rand() % 7 + 1, size);

	while (n--)
		*c++ = '0' + rand() % 10;
	return c;
}

static void genline(char *buf, int size)
{
	char *c = buf;
	char *stop = buf + size - 1;

	while (c < stop - 4) {
		if (c != buf)
			*c++ = ' ';
		if (rand() % 15)
			c = genword(c, stop - c);
		else
			c = gennumber(c, stop - c);
	}
	*c++ = '\0';
}

static int findtypos(char *typos, char *text, char *typed, int len)
{
	int i;
	int n = 0;
	for (i = 0; i < len; i++)
		typos[i] = text[i] == typed[i] ? ' ' : '^';
	for (i = 0; i < len; i++)
		if (typos[i] != ' ')
			n++;
	typos[len] = '\0';
	return n;
}

static void printstat(int accx100, int ratex100)
{
	printf("\n  accuracy = %d.%d,  speed = %d.%dwpm\n\n",
		accx100 / 100, accx100 % 100,
		ratex100 / 100, ratex100 % 100);
}

static void lineget(char *dst, int len)
{
	int i = 0;
	while (i < len && read(0, dst + i, 1) == 1 && dst[i] != '\n')
		i++;
	dst[i] = '\0';
}

int main(void)
{
	char text[BUFSZ];
	char typed[BUFSZ];
	char typos[BUFSZ];
	struct timeval beg, end;
	long duration;
	int all = 0, seen = 0, typo = 0;
	int i;

	srand(time(NULL));
	gettimeofday(&beg, NULL);
	for (i = 0; i < 4; i++) {
		genline(text, LINELEN - rand() % 8);
		printf("%s\n", text);
		memset(typed, 0, sizeof(typed));
		lineget(typed, sizeof(typed));
		all += strlen(text);
		typo += findtypos(typos, text, typed, strlen(text));
		seen += strlen(typed);
		printf("%s\n", typos);
	}
	gettimeofday(&end, NULL);
	duration = (end.tv_sec - beg.tv_sec) * 10 +
			(end.tv_usec - beg.tv_usec) / 100000;
	printstat((all - typo) * 10000 / all, seen * 60 * 1000 / (duration * 6));
	if (typo)
		printf("repeat after me!\n");
	else
		printf("cool!\n");
	return 0;
}
