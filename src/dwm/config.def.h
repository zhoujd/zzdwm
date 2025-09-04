/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 1;   /* border pixel of windows */
static const unsigned int snap      = 32;  /* snap pixel */
static const unsigned int deftag    = 1;   /* default tag on startup, 1 means default */
static const int startwithgaps      = 1;   /* 1 means gaps are used by default */
static const int gappx              = 5;   /* default gap between windows in pixels */
static const int maxgappx           = 50;  /* max gap between windows in pixels */
static const int vertgappx          = 4;   /* default vertical gap of a client for smfact in tile layout */
static const int showbar            = 0;   /* 0 means no bar */
static const int topbar             = 1;   /* 0 means bottom bar */
static const int barpad             = 8;   /* 0 means use default bar padding */
static const int horizpadbar        = 4;   /* horizontal padding for statusbar in pixels */
static const int vertpadbar         = 0;   /* vertical padding for statusbar in pixels */
static const int focusonwheel       = 0;   /* 0 means focus window by mouse click */
static const int viewontag          = 1;   /* 1 means switch view on tag switch */
static const int attachmode         = 0;   /* 0 master (default), 1 = above, 2 = aside, 3 = below, 4 = bottom */
static const int background         = 1;   /* 0 means no root background */
static const int urgentontag        = 0;   /* 1 means urgent show on tag */
static const int mincellw           = 200; /* min cell width in grid and resizing in float */
static const int mincellh           = 120; /* min cell height in grid and resizing in float */
static const int showstatus         = 2;   /* 0 means no status text, 1 = selected (default), 2 = all monitors */
static const int floatoffset        = 1;   /* 0 default means no float window offset base on bar height */
static const int centertitle        = 1;   /* 1 means place title in the middle of barwin, 0 means default */
static const int maxtitle           = 75;  /* max title length in bytes, 0 means no title */
static const float minwfact         = 2.0; /* min window fact in tile and bstack */
static const char *fonts[]          = {
	"SF Mono:size=11",
	"PingFang SC:size=11",
	"Symbols Nerd Font Mono:size=11",
};
/* select the font index for statusbar
 * the index is zero based
 */
static const int statusfontindex    = 0;
/* colors */
#include "themes/custom.h"
static const char *colors[][3]      = {
	/*                 fg         bg         border   */
	[SchemeNorm]   = { col_gray3, col_gray1, col_gray2 },
	[SchemeSel]    = { col_gray4, col_cyan,  col_cyan  },
	[SchemeTitle]  = { col_gray4, col_cyan,  col_cyan  },
	[SchemeStatus] = { col_gray3, col_gray1, col_gray2 },
};
/* alpha */
static const unsigned int baralpha    = 0xd0;
static const unsigned int borderalpha = OPAQUE;
static const unsigned int alphas[][3] = {
	/*                 fg      bg        border*/
	[SchemeNorm]   = { OPAQUE, baralpha, borderalpha },
	[SchemeSel]    = { OPAQUE, baralpha, borderalpha },
	[SchemeTitle]  = { OPAQUE, baralpha, borderalpha },
	[SchemeStatus] = { OPAQUE, baralpha, borderalpha },
};

/* auto start */
static const char *const autostart[] = {
	"blackwalls", NULL,
	"dwmstatus", NULL,
	"shotkey", NULL,
	"xbanish", NULL,
	"xcompmgr", NULL,
	NULL /* terminate */
};
/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };
static const int taglayouts[] = { 2, 0, 0, 0, 0, 0, 0, 0, 0 };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ "Gimp",     NULL,       NULL,       0,            1,           -1 },
	{ "Firefox",  NULL,       NULL,       1 << 8,       0,           -1 },
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const float smfact    = 0.00; /* factor of tiled clients [0.00..0.90] */
static const float dmfact    = 0.50; /* factor of doubledeck master area size [0.05..0.95] */
static const float zmfact    = 0.50; /* factor of zetadeck master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int nmaxmaster  = 4;    /* max number of clients in master area */
static const int nminmaster  = 0;    /* min number of clients in master area */
static const int resizehints = 0;    /* 1 means respect size hints in tiled resizals */
static const int floathints  = 0;    /* 1 means respect size hints if the window is floating */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },    /* first entry is default */
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
	{ "HHH",      grid },
	{ "[D]",      doubledeck },
	{ "[Z]",      zetadeck },
	{ "TTT",      bstack },
	{ "[ ]",      clear },   /* hides all visible clients */
	{ NULL,       NULL },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|Mod1Mask,              KEY,      swaptag,        {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }
#define CMD(...)   { .v = (const char*[]){ __VA_ARGS__, NULL } }

/* commands */
static const char *termcmd[] = { "st", NULL };
static const char *emacscmd[] = { "runec", NULL };

/* XFree86 vender specific keysyms */
#include <X11/XF86keysym.h>
/* keys */
static const Key keys[] = {
	/* modifier                     key              function          argument */
	{ MODKEY,                       XK_c,            spawn,            {.v = termcmd} },
	{ MODKEY,                       XK_e,            spawn,            {.v = emacscmd} },
	{ MODKEY|ShiftMask,             XK_b,            togglebar,        {0} },
	{ MODKEY,                       XK_i,            incnmaster,       {.i = +1} },
	{ MODKEY,                       XK_d,            incnmaster,       {.i = -1} },
	{ MODKEY|ShiftMask,             XK_i,            resetnmaster,     {.ui = nmaxmaster} },
	{ MODKEY|ShiftMask,             XK_d,            resetnmaster,     {.ui = nminmaster} },
	{ MODKEY,                       XK_Return,       zoom,             {0} },
	{ MODKEY,                       XK_Tab,          view,             {0} },
	{ MODKEY|ShiftMask,             XK_x,            killclient,       {0} },
	{ MODKEY|ControlMask,           XK_x,            killclient,       {.ui = 1} }, /* others in tag */
	{ MODKEY|ControlMask|ShiftMask, XK_x,            killclient,       {.ui = 2} }, /* all in tag */
	{ MODKEY,                       XK_t,            setlayout,        {.v = &layouts[0]} },
	{ MODKEY,                       XK_f,            setlayout,        {.v = &layouts[1]} },
	{ MODKEY,                       XK_m,            setlayout,        {.v = &layouts[2]} },
	{ MODKEY,                       XK_g,            setlayout,        {.v = &layouts[3]} },
	{ MODKEY,                       XK_x,            setlayout,        {.v = &layouts[4]} },
	{ MODKEY,                       XK_z,            setlayout,        {.v = &layouts[5]} },
	{ MODKEY,                       XK_b,            setlayout,        {.v = &layouts[6]} },
	{ MODKEY,                       XK_BackSpace,    setlayout,        {.v = &layouts[7]} },
	{ MODKEY,                       XK_space,        setlayout,        {0} },
	{ MODKEY|ShiftMask,             XK_space,        togglefloating,   {0} },
	{ MODKEY|ControlMask,           XK_space,        focusmaster,      {0} },
	{ MODKEY|ControlMask|ShiftMask, XK_space,        resetmfact,       {0} },
	{ MODKEY,                       XK_o,            setlayout,        {0} },
	{ MODKEY|ShiftMask,             XK_o,            togglefloating,   {0} },
	{ MODKEY|ControlMask,           XK_o,            focusmaster,      {0} },
	{ MODKEY|ControlMask|ShiftMask, XK_o,            resetmfact,       {0} },
	{ MODKEY|ShiftMask,             XK_f,            togglefullscr,    {0} },
	{ MODKEY|ShiftMask,             XK_s,            togglesticky,     {0} },
	{ MODKEY,                       XK_u,            swapfocus,        {0} },
	{ MODKEY,                       XK_0,            view,             {.ui = ~0} },
	{ MODKEY|ShiftMask,             XK_0,            tag,              {.ui = ~0} },
	{ MODKEY,                       XK_minus,        setmfact,         {.f = -0.05} },
	{ MODKEY,                       XK_equal,        setmfact,         {.f = +0.05} },
	{ MODKEY|ShiftMask,             XK_minus,        setgaps,          {.i = -5} },
	{ MODKEY|ShiftMask,             XK_equal,        setgaps,          {.i = +5} },
	{ MODKEY|ControlMask,           XK_minus,        setgaps,          {.i = GAP_RESET} },
	{ MODKEY|ControlMask,           XK_equal,        setgaps,          {.i = GAP_TOGGLE} },
	{ MODKEY,                       XK_comma,        focusmon,         {.i = -1} },
	{ MODKEY,                       XK_period,       focusmon,         {.i = +1} },
	{ MODKEY|ShiftMask,             XK_comma,        tagmon,           {.i = -1} },
	{ MODKEY|ShiftMask,             XK_period,       tagmon,           {.i = +1} },
	{ MODKEY|ControlMask,           XK_comma,        tagswapmon,       {.i = -1} },
	{ MODKEY|ControlMask,           XK_period,       tagswapmon,       {.i = +1} },
	{ MODKEY,                       XK_bracketleft,  cyclelayout,      {.i = -1} },
	{ MODKEY,                       XK_bracketright, cyclelayout,      {.i = +1} },
	{ MODKEY,                       XK_Left,         rotatetags,       {.i = -1} },
	{ MODKEY,                       XK_Right,        rotatetags,       {.i = +1} },
	{ MODKEY,                       XK_Up,           rotatestack,      {.i = -1} },
	{ MODKEY,                       XK_Down,         rotatestack,      {.i = +1} },
	{ MODKEY,                       XK_h,            shiftviewclients, {.i = -1} },
	{ MODKEY,                       XK_l,            shiftviewclients, {.i = +1} },
	{ MODKEY,                       XK_j,            focusstack,       {.i = +1} },
	{ MODKEY,                       XK_k,            focusstack,       {.i = -1} },
	{ MODKEY|ShiftMask,             XK_h,            setmfact,         {.f = -0.05} },
	{ MODKEY|ShiftMask,             XK_l,            setmfact,         {.f = +0.05} },
	{ MODKEY|ShiftMask,             XK_j,            setsmfact,        {.f = +0.05} },
	{ MODKEY|ShiftMask,             XK_k,            setsmfact,        {.f = -0.05} },
	{ MODKEY|ShiftMask,             XK_m,            fmaximize,        {.ui = 0} }, /* maximize */
	{ MODKEY,                       XK_backslash,    fmaximize,        {.ui = 1} }, /* full width */
	{ MODKEY|ShiftMask,             XK_backslash,    fmaximize,        {.ui = 2} }, /* full height */
	{ MODKEY|ControlMask|ShiftMask, XK_h,            setwfact,         {.f = -0.05} },
	{ MODKEY|ControlMask|ShiftMask, XK_l,            setwfact,         {.f = +0.05} },
	{ MODKEY|ControlMask|ShiftMask, XK_j,            sethfact,         {.f = +0.05} },
	{ MODKEY|ControlMask|ShiftMask, XK_k,            sethfact,         {.f = -0.05} },
	{ MODKEY|Mod1Mask,              XK_h,            fsnap,            {.ui = 0} }, /* left */
	{ MODKEY|Mod1Mask,              XK_j,            fsnap,            {.ui = 1} }, /* down */
	{ MODKEY|Mod1Mask,              XK_k,            fsnap,            {.ui = 2} }, /* up */
	{ MODKEY|Mod1Mask,              XK_l,            fsnap,            {.ui = 3} }, /* right */
	{ MODKEY|ControlMask,           XK_h,            fdeck,            {.ui = 0} }, /* left */
	{ MODKEY|ControlMask,           XK_j,            fdeck,            {.ui = 1} }, /* bottom */
	{ MODKEY|ControlMask,           XK_k,            fdeck,            {.ui = 2} }, /* top */
	{ MODKEY|ControlMask,           XK_l,            fdeck,            {.ui = 3} }, /* right */
	{ MODKEY,                       XK_v,            focussame,        {.i = +1} },
	{ MODKEY|ShiftMask,             XK_v,            focussame,        {.i = -1} },
	TAGKEYS(                        XK_1,                              0)
	TAGKEYS(                        XK_2,                              1)
	TAGKEYS(                        XK_3,                              2)
	TAGKEYS(                        XK_4,                              3)
	TAGKEYS(                        XK_5,                              4)
	TAGKEYS(                        XK_6,                              5)
	TAGKEYS(                        XK_7,                              6)
	TAGKEYS(                        XK_8,                              7)
	TAGKEYS(                        XK_9,                              8)
	{ MODKEY|ControlMask,           XK_q,            quit,             {0} },
	{ MODKEY|ControlMask|ShiftMask, XK_q,            quit,             {1} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click             event mask          button          function          argument */
	{ ClkLtSymbol,       0,                  Button1,        setlayout,        {0} },
	{ ClkLtSymbol,       0,                  Button3,        setlayout,        {.v = &layouts[2]} },
	{ ClkWinTitle,       0,                  Button2,        zoom,             {0} },
	{ ClkStatusText,     0,                  Button2,        spawn,            {.v = termcmd} },
	{ ClkClientWin,      MODKEY,             Button1,        moveorplace,      {.i = 1} },
	{ ClkClientWin,      MODKEY,             Button2,        togglefloating,   {0} },
	{ ClkClientWin,      MODKEY,             Button3,        resizemouse,      {0} },
	{ ClkTagBar,         0,                  Button1,        view,             {0} },
	{ ClkTagBar,         0,                  Button3,        toggleview,       {0} },
	{ ClkTagBar,         MODKEY,             Button1,        tag,              {0} },
	{ ClkTagBar,         MODKEY,             Button3,        toggletag,        {0} },
	{ ClkTagBar,         0,                  Button4,        rotatetags,       {.i = -1} },
	{ ClkTagBar,         0,                  Button5,        rotatetags,       {.i = +1} },
	{ ClkTagBar,         MODKEY,             Button4,        shiftviewclients, {.i = -1} },
	{ ClkTagBar,         MODKEY,             Button5,        shiftviewclients, {.i = +1} },
};
