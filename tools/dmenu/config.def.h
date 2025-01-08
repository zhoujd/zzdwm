/* See LICENSE file for copyright and license details. */
/* Default settings; can be overriden by command line. */

static int topbar = 1;                         /* -b option; if 0, dmenu appears at bottom     */
static int centered = 1;                       /* -c option; centers dmenu on screen */
static int min_width = 500;                    /* minimum width when centered */
static int max_width = 640;                    /* maximum width when centered; if 0, not apply */
static int colorprompt = 1;                    /* if 1, prompt uses SchemeSel, otherwise SchemeNorm */
static const int user_bh = 2;                  /* add an defined amount of pixels to the bar height */
static const float menu_height_ratio = 4.0f;   /* This is the ratio used in the original calculation */
/* -fn option overrides fonts[0]; default X11 font or font set */
static const char *fonts[] = {
	"SF Mono:size=11",
	"SF Mono SC:size=11",
};
/* -p option; prompt to the left of input field */
static const char *prompt = NULL;
static const char *colors[SchemeLast][2] = {
	/*     fg         bg       */
	[SchemeNorm] = { "#bbbbbb", "#222222" },
	[SchemeSel] = { "#eeeeee", "#005577" },
	[SchemeSelHighlight] = { "#ffc978", "#005577" },
	[SchemeNormHighlight] = { "#ffc978", "#222222" },
	[SchemeOut] = { "#000000", "#00ffff" },
	[SchemeOutHighlight] = { "#ffc978", "#00ffff" },
};
/* -l option; if nonzero, dmenu uses vertical list with given number of lines */
static unsigned int lines = 0;

/*
 * Characters not considered part of a word while deleting words
 * for example: " /?\"&[]"
 */
static const char worddelimiters[] = " ";

/* -bw option; the size of the window border */
static unsigned int border_width = 0;
