/*
 * Copyright (c) 2002-2020 Jean-Francois Moine.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ed.h"
#if COLOR
#include "color.h"
#undef __GNUC__
#include "fontlock_cpp.c"

/* Font lock anchors. */
#define ANCHOR_NULL 0
#define ANCHOR_C_COMMENT 1
#define ANCHOR_XML_COMMENT 2
#define ANCHOR_XML_TEXT 3
#define ANCHOR_STRING 4
#define ANCHOR_QUOTE 5
#define ANCHOR_BQUOTE 6
#define ANCHOR_PS_STRING 7

#define BMODE_C		1
#define BMODE_SHELL	2		/* comment = '#' */
#define BMODE_PS	3		/* comment = '%' */
#define BMODE_LISP	4		/* comment = ';' */
#define BMODE_X		5		/* comment = '!' */
#define BMODE_XML	6
#define BMODE_ABC	7		/* comment = '%' */

static unsigned char comment_tb[] = { 0, 0, '#', '%', ';', '!' , 0, '%'};

/*
 * Parse the line for comments and strings.
 */
static int set_anchors(BUFFER *bp,
		       LINE *lp,
		       int lastanchor)
{
	unsigned char *sp, *spe, sep;
	unsigned char comment;

	if (llength(lp) == 0)
		return lastanchor;

	comment = comment_tb[bp->b_mode];
	sp = lp->l_text;
	spe = sp + llength(lp);
	while (sp < spe) {
		switch (lastanchor) {
		case ANCHOR_QUOTE:
		case ANCHOR_STRING:
		case ANCHOR_BQUOTE:
			switch (lastanchor) {
			case ANCHOR_QUOTE: sep = '\''; break;
			case ANCHOR_STRING: sep = '"'; break;
			default: sep = '`'; break;
			}
			while (sp < spe) {
				if (*sp++ == '\\') {
					sp++;
				} else if (sp[-1] == sep) {
					lastanchor = ANCHOR_NULL;
					break;
				}
			}
			break;
		case ANCHOR_C_COMMENT:
			while (sp < spe) {
				if (*sp++ == '*') {
					if (sp >= spe)
						return lastanchor;
					if (*sp != '/')
						continue;
					sp++;
					lastanchor = ANCHOR_NULL;
					break;
				}
			}
			break;
		case ANCHOR_XML_COMMENT:
			while (sp < spe) {
				if (*sp++ == '-') {
					if (sp >= spe - 1)
						return lastanchor;
					if (*sp != '-' || sp[1] != '>')
						continue;
					sp += 2;
					lastanchor = ANCHOR_XML_TEXT;
					break;
				}
			}
			if (sp >= spe)
				return lastanchor;
			/* FALLTHRU */
		case ANCHOR_XML_TEXT:
			while (sp < spe) {
				if (*sp == '<')
					break;
				sp++;
			}
			if (sp >= spe)
				return lastanchor;
			lastanchor = ANCHOR_NULL;
			if (sp >= spe - 2)
				return lastanchor;
			if (*sp != '!'
			 || sp[1] != '-'
			 || sp[2] != '-')
				break;
			sp += 3;
			lastanchor = ANCHOR_XML_COMMENT;
			continue;
		}

		/* ANCHOR_NULL */
		while (sp < spe) {
			switch (*sp++) {
			case '\\':
				sp++;
				continue;
			case '#':
			case '%':
			case ';':
			case '!':
				if (sp[-1] == comment)
					return lastanchor;
				continue;
			case '/':
				if (bp->b_mode != BMODE_C)
					continue;
				if (sp >= spe)
					return lastanchor;
				if (*sp == '/')
					return lastanchor;
				if (*sp != '*')
					continue;
				sp++;
				lastanchor = ANCHOR_C_COMMENT;
				break;
			case '\'':
				if (bp->b_mode == BMODE_PS
				 || bp->b_mode == BMODE_ABC)
					continue;
				lastanchor = ANCHOR_QUOTE;
				break;
			case '"':
				if (bp->b_mode == BMODE_PS)
					continue;
				lastanchor = ANCHOR_STRING;
				break;
			case '`':
				if (bp->b_mode > BMODE_SHELL)
					continue;
				lastanchor = ANCHOR_BQUOTE;
				break;
			case '>':
				if (bp->b_mode != BMODE_XML)
					continue;
				lastanchor = ANCHOR_XML_TEXT;
				break;
			case '(':
				if (bp->b_mode != BMODE_PS)
					continue;
				lastanchor = ANCHOR_PS_STRING;
				break;
			default:
				continue;
			}
			break;
		}
	}
	return lastanchor;
}

/*
 * Go to the beginning of the buffer and parse for anchors.
 */
static void font_lock_set_anchors(BUFFER *bp)
{
	LINE *lp;
	int lastanchor;

	lastanchor = bp->b_mode != BMODE_XML ? ANCHOR_NULL : ANCHOR_XML_TEXT;

	lp = lforw(bp->b_linep);
	lp->anchor = lastanchor;
	for (;;) {
		lastanchor = set_anchors(bp, lp, lastanchor);
		lp = lforw(lp);
		if (lp == bp->b_linep)
			break;
		lp->anchor = lastanchor;
	}
}

/*
 * Go to the beginning of the buffer and remove the anchors.
 */
static void font_lock_unset_anchors(BUFFER *bp)
{
	LINE *lp;

	for (lp = lforw(bp->b_linep); lp != bp->b_linep; lp = lforw(lp))
		lp->anchor = 0;
}

/*
 * Toggle Font Lock mode.
 * When Font Lock mode is enabled, text is fontified as you type it.
 */
static int font_lock_mode(int f, int n)
{
	if (curbp->b_flag & BFFLCK) {
		curbp->b_flag &= ~BFFLCK;
		font_lock_unset_anchors(curbp);
	} else {
		curbp->b_flag |= BFFLCK;
		font_lock_set_anchors(curbp);
	}
	return TRUE;
}

/*
 * Check the file extension.
 */
static int has_extension(char *ext, char **exts)
{
	while (*exts != 0) {
		if (strcmp(ext, *exts) == 0)
			return TRUE;
		exts++;
	}
	return FALSE;
}

/*
 * Guess the buffer mode from the first characters.
 */
static int guess_mode(void)
{
	LINE *lp;

	lp = lforw(curbp->b_linep);
	if (llength(lp) == 0)
		return 0;
	switch (lp->l_text[0]) {
	case '#':
	case ':':
		return BMODE_SHELL;
	case '/':
		if (llength(lp) < 2)
			return 0;
		if (lp->l_text[1] == '/'
		 || lp->l_text[1] == '*')
			return BMODE_C;
		break;
	case '%':
		if (llength(lp) < 2)
			return 0;
		if (lp->l_text[1] == 'a')
			return BMODE_ABC;
		return BMODE_PS;
	case ';':
		return BMODE_LISP;
	case '!':
		return BMODE_X;
	case '<':
		return BMODE_XML;
	}
	return 0;
}

/*
 * Set the file mode.
 */
void set_filemode(char *fname)
{
	static char *c_file[] = { "c", "h",
				 "C", "H", "cc", "cpp",
				 "cxx", "hpp", "js", 0 };
	static char *shell_file[] = { "sh", "csh", "py",
				     "tcl", "tk", "pl", 0 };
	static char *abc_file[] = { "abc", "fmt", 0 };
	static char *ps_file[] = { "ps", "eps", 0 };
	static char *lisp_file[] = { "lsp", "el", 0 };
	char *ext;

	curbp->b_mode = 0;
	ext = strrchr(fname, '.');
	if (ext != 0) {
		ext++;
		if (has_extension(ext, c_file))
			curbp->b_mode = BMODE_C;
		else if (has_extension(ext, shell_file))
			curbp->b_mode = BMODE_SHELL;
		else if (has_extension(ext, abc_file))
			curbp->b_mode = BMODE_ABC;
		else if (has_extension(ext, ps_file))
			curbp->b_mode = BMODE_PS;
		else if (has_extension(ext, lisp_file))
			curbp->b_mode = BMODE_LISP;
	}
	if (curbp->b_mode == 0)
		curbp->b_mode = guess_mode();
	if (curbp->b_mode != 0)
		font_lock_mode(0, 0);
}

/* -- adapted from zile/ncurses_redisplay.c -- */

/*
 * Colors
 */
static int font_comment = COLOR_COMMENT << ATTR_SHIFT;
static int font_directive = COLOR_DIRECTIVE << ATTR_SHIFT;
static int font_identifier = COLOR_IDENTIFIER << ATTR_SHIFT;
static int font_keyword = COLOR_KEYWORD << ATTR_SHIFT;
static int font_number = COLOR_NUMBER << ATTR_SHIFT;
static int font_other = COLOR_OTHER << ATTR_SHIFT;
static int font_string = COLOR_STRING << ATTR_SHIFT;
static int font_string_delimiters = COLOR_STRING_DELIMITERS << ATTR_SHIFT;

#ifdef UTF8
static unsigned char *put_attr_char(int font,
				unsigned char c,	/* first byte */
				unsigned char *sp)	/* next byte */
{
	if (c > 0xc0) {
		if (c > 0xe0) {
			unsigned char c2;

			c2 = *sp++;
			vtputc(font + (c << 16) + (c2 << 8) + *sp++);
		} else {
			vtputc(font + (c << 8) + *sp++);
		}
	} else {
		vtputc(font + c);
	}
	return sp;
}
#endif

static void put_attr_char_seq(int font,
			   unsigned char *sp,
			   unsigned char *spe)
{
	unsigned char c;

	while (sp < spe) {
		c = *sp++;
#ifdef UTF8
		sp = put_attr_char(font, c, sp);
#else
		vtputc(font + c);
#endif
	}
}

/* -- draw a line in the virtual screen (see display.c) -- */
int draw_line(BUFFER *bp, LINE *lp)
{
	int mode;
	unsigned char *sp, *spe;
	unsigned char c, sep;
	char lastanchor;
	int font;

	sp = lp->l_text;
	spe = sp + llength(lp);

	mode = bp->b_flag & BFFLCK ? bp->b_mode : 0;

	if (mode == 0) {
		put_attr_char_seq(COLOR_NORMAL << ATTR_SHIFT, sp, spe);
		return ANCHOR_NULL;
	}

	lastanchor = lp->anchor;
	if (mode != BMODE_C) {
		char comment;

		comment = comment_tb[bp->b_mode];
		while (sp < spe) {
			switch (lastanchor) {
			case ANCHOR_NULL:
				font = font_other;
				while (sp < spe) {
					c = *sp++;
// should treat here the language specific keywords
					if (c == comment) {
						if (mode == BMODE_ABC
						 && ((sp == lp->l_text + 1
						   && *sp == comment)
						  || (sp == lp->l_text + 2
						   && sp[-2] == comment))) {
							vtputc(font_directive + c);
							while (sp < spe
							    && !isspace(*sp))
								vtputc(font_directive
									+ *sp++);
							continue;
						}
						put_attr_char_seq(font_comment,
								sp - 1, spe);
						return lastanchor;
					}
					if (mode == BMODE_PS) {
						if (c == '(') {
							vtputc(font_string_delimiters +
									c);
							lastanchor = ANCHOR_PS_STRING;
							break;
						}
					} else if (c == '"') {
						vtputc(font_string_delimiters +
								c);
						lastanchor = ANCHOR_STRING;
						break;
					} else if (c == '\'') {
						if (mode != BMODE_ABC) {
							vtputc(font_string_delimiters + c);
							lastanchor = ANCHOR_QUOTE;
							break;
						}
					} else if (c == '\\') {
						vtputc(font + c);
						if (sp >= spe)
							return lastanchor;
						c = *sp++;
					} else if (c == '`') {
						if (mode == BMODE_SHELL) {
							vtputc(font_string_delimiters +
								c);
							lastanchor = ANCHOR_BQUOTE;
							break;
						}
					} else if (c == '>') {
						if (mode == BMODE_XML) {
							vtputc(font + c);
							lastanchor = ANCHOR_XML_TEXT;
							break;
						}
					}
#ifdef UTF8
					sp = put_attr_char(font, c, sp);
#else
					vtputc(font + c);
#endif
				}
				continue;
			case ANCHOR_STRING:
				sep = '"';
				break;
			case ANCHOR_QUOTE:
				sep = '\'';
				break;
			case ANCHOR_BQUOTE:
				sep = '`';
				break;
			case ANCHOR_PS_STRING:
				sep = ')';
				break;
			case ANCHOR_XML_COMMENT:
				font = font_comment;
				while (sp < spe) {
					c = *sp++;
#ifdef UTF8
					sp = put_attr_char(font, c, sp);
#else
					vtputc(font + c);
#endif
					if (c == '-') {
						if (sp >= spe - 1)
							continue;
						if (*sp != '-' || sp[1] != '>')
							continue;
						vtputc(font + *sp++);
						vtputc(font + *sp++);
						lastanchor = ANCHOR_XML_TEXT;
						break;
					}
				}
				if (sp < spe)
					continue;
				return lastanchor;
			case ANCHOR_XML_TEXT:
				font = font_other;
				for (;;) {
					c = *sp++;
					if (c == '<') {
						lastanchor = ANCHOR_NULL;
						if (sp < spe - 2
						 && *sp == '!'
						 && sp[1] == '-'
						 && sp[2] == '-') {
							vtputc(font_comment + c);
							lastanchor = ANCHOR_XML_COMMENT;
							break;
						}
						vtputc(font + c);
						if (sp >= spe)
							return lastanchor;
						if (!isalpha(*sp))
							vtputc(font + *sp++);
						while (sp < spe
						    && isalnum(*sp))
							vtputc(font_keyword + *sp++);
						break;
					}
#ifdef UTF8
					sp = put_attr_char(font, c, sp);
#else
					vtputc(font + c);
#endif
					if (sp >= spe)
						return lastanchor;
				}
				continue;
			default:
				lastanchor = ANCHOR_NULL;
				continue;
			}

			/* string/quote/bquote */
			font = font_string;
			while (sp < spe) {
				c = *sp++;
				if (c == sep) {
					vtputc(font_string_delimiters + c);
					lastanchor = ANCHOR_NULL;
					break;
				}
				if (c == '\\' && sp < spe) {
					vtputc(font + c);
					c = *sp++;
				}
#ifdef UTF8
				sp = put_attr_char(font, c, sp);
#else
				vtputc(font + c);
#endif
			}
		}
		return lastanchor;
	}

	/* (mode == BMODE_C) */
	while (sp < spe) {
		switch (lastanchor) {
		case ANCHOR_NULL:
			font = font_other;
			while (sp < spe) {
				c = *sp++;
				if (c == '"') {
					vtputc(font_string_delimiters + c);
					lastanchor = ANCHOR_STRING;
					break;
				}
				if (c == '\'') {
					vtputc(font_string_delimiters + c);
					lastanchor = ANCHOR_QUOTE;
					break;
				}
				if (c == '/'
				 && sp < spe) {
					if (*sp == '/') {
						/* C++ comment */
						put_attr_char_seq(font_comment,
								sp - 1, spe);
						return lastanchor;
					}
					if (*sp == '*') {
						vtputc(font_comment + c);
						vtputc(font_comment + *sp++);
						lastanchor = ANCHOR_C_COMMENT;
						break;
					}
					vtputc(font + c);
					continue;
				}
				if (c == '#') {
					vtputc(font_directive + c);
					while (sp < spe
					    && isspace(*sp))
						vtputc(font + *sp++);
					while (sp < spe
					    && isalpha(*sp))
						vtputc(font_directive + *sp++);
					continue;
				}
				if (isalpha(c) || c == '_') {
					unsigned char *spl;
					int attr;

					spl = sp;
					sp--;
					while (spl < spe
					    && (isalnum(*spl)
					     || *spl == '_'))
						spl++;
					if (is_cpp_keyword((char *) sp, spl - sp) != 0)
						attr = font_keyword;
					else
						attr = font_identifier;
					while (sp < spl)
						vtputc(attr + *sp++);
					continue;
				}
				if (isdigit(c)
				 || c == '-' || c == '.') { /* Integer or float. */
					/* XXX Fix this to support floats correctly. */
					if ((c == '.' || c == '-')
					 && (sp >= spe || !isdigit(*sp))) {
						vtputc(font + c);
						continue;
					}
					vtputc(font_number + c);
					while (sp < spe
					    && (isxdigit(*sp)
					     || strchr("xeE.+-lLfFuU",
							     *sp) != NULL))
						vtputc(font_number + *sp++);
					continue;
				}
#ifdef UTF8
				sp = put_attr_char(font, c, sp);
#else
				vtputc(font + c);
#endif
			}
			continue;
		case ANCHOR_C_COMMENT:
			font = font_comment;
			while (sp < spe) {
				c = *sp++;
#ifdef UTF8
				sp = put_attr_char(font, c, sp);
#else
				vtputc(font + c);
#endif
				if (c == '*'
				 && sp < spe
				 && *sp == '/') {
					vtputc(font_comment + *sp++);
					lastanchor = ANCHOR_NULL;
					break;
				}
			}
			continue;
		case ANCHOR_STRING:
			sep = '"';
			break;
		case ANCHOR_QUOTE:
			sep = '\'';
			break;
		default:
			lastanchor = ANCHOR_NULL;
			continue;
		}
		font = font_string;
		while (sp < spe) {
			c = *sp++;
			if (c == sep) {
				vtputc(font_string_delimiters + c);
				lastanchor = ANCHOR_NULL;
				break;
			}
			if (c == '\\' && sp < spe) {
				vtputc(font + c);
				c = *sp++;
			}
#ifdef UTF8
			sp = put_attr_char(font, c, sp);
#else
			vtputc(font + c);
#endif
		}
	}
	return lastanchor;
}
#endif /*COLOR*/
