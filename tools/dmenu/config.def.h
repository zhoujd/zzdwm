/* See LICENSE file for copyright and license details. */
/* Default settings; can be overriden by command line. */

static int topbar = 1;                         /* -b option; if 0, dmenu appears at bottom     */
static int centered = 1;                       /* -c option; centers dmenu on screen */
static int min_width = 500;                    /* minimum width when centered */
static int max_width = 640;                    /* maximum width when centered; if 0, not apply */
static int colorprompt = 1;                    /* if 1, prompt uses SchemeSel, otherwise SchemeNorm */
static const int user_bh = 2;                  /* add an defined amount of pixels to the bar height */
static const float menu_height_ratio = 3.0f;   /* This is the ratio used in the original calculation */
/* -fn option overrides fonts[0]; default X11 font or font set */
static const char *fonts[] = {
	"SF Mono:size=11",
	"PingFang SC:size=11",
	"Symbols Nerd Font:size=11",
};
/* -p option; prompt to the left of input field */
static const char *prompt = NULL;
#include "themes/custom.h"
static const char *colors[SchemeLast][2] = {
	/*     fg         bg       */
	[SchemeNorm] = { norm_fg, norm_bg },
	[SchemeSel]  = { sel_fg,  sel_bg },
	[SchemeOut]  = { out_fg,  out_bg },
	[SchemeNormHighlight] = { norm_hi_fg, norm_bg },
	[SchemeSelHighlight]  = { sel_hi_fg,  sel_bg },
	[SchemeOutHighlight]  = { out_hi_fg,  out_bg },
};
/* -l option; if nonzero, dmenu uses vertical list with given number of lines */
static unsigned int lines = 0;

/*
 * Characters not considered part of a word while deleting words
 * for example: " /?\"&[]"
 */
static const char worddelimiters[] = " ";

/* -n option; preselected item starting from 0 */
static unsigned int preselected = 0;

/* -bw option; the size of the window border */
static unsigned int border_width = 0;

/* -r option; if 1, disables shift-return and ctrl-return */
static int restrict_return = 0;
static const char *restrict_value = "NEW";
