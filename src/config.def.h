/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 1;        /* border pixel of windows */
static const int startwithgaps	    = 1;        /* 1 means gaps are used by default */
static const unsigned int gappx     = 10;       /* default gap between windows in pixels */
static const unsigned int snap      = 32;       /* snap pixel */
static const unsigned int minwsz    = 20;       /* Minimal heigt of a client for smfact */
static const int showbar            = 0;        /* 0 means no bar */
static const int topbar             = 1;        /* 0 means bottom bar */
static const int horizpadbar        = 4;        /* horizontal padding for statusbar */
static const int vertpadbar         = 0;        /* vertical padding for statusbar */
static const int focusonwheel       = 0;        /* 0 means focus window by mouse click */
static const int viewontag          = 1;        /* Switch view on tag switch */
static const int attachmode         = 0;        /* 0 master (default), 1 = above, 2 = aside, 3 = below, 4 = bottom */
static const int background         = 1;        /* 0 means no root background */
static const int focusedontoptiled  = 1;        /* 1 means focused tile client is shown on top of floating windows */
static const int pertagallbars      = 0;        /* 1 means pertag with the same barpos */
static const char *fonts[]          = {
	"SF Pro Display:size=11",
	"SF Mono:size=11",
	"SF Mono SC:size=11",
};
/* select the font index for statusbar
 * the index is zero based */
static const int statusfontindex    = 0;
/* colors */
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#005577";
static const char *colors[][3]      = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
	[SchemeSel]  = { col_gray4, col_cyan,  col_cyan  },
};
/* auto start */
static const char *const autostart[] = {
	"dwmstatus", "&", NULL,
	"shotkey", "&", NULL,
	"xbanish", "&", NULL,
	NULL /* terminate */
};
/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5", "6", "7", "8", "9" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   alwaysontop  monitor */
	{ "Gimp",     NULL,       NULL,       0,            1,           0,           -1 },
	{ "Firefox",  NULL,       NULL,       1 << 8,       0,           0,           -1 },
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const float smfact    = 0.00; /* factor of tiled clients [0.00..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int nmaxmaster  = 4;    /* max number of clients in master area */
static const int nminmaster  = 0;    /* min number of clients in master area */
static const int resizehints = 0;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },    /* first entry is default */
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
	{ "HHH",      grid },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static const char *termcmd[] = { "st", NULL };
static const char *pulltermcmd[] = { "dmenu_app", "st", NULL };
static const char *emacscmd[] = { "emacs", NULL };
static const char *pullemacscmd[] = { "dmenu_app", "emacs", NULL };
static const char *browsercmd[] = { "dmenu_drun", "google-chrome", NULL };
static const char *codecmd[] = { "dmenu_drun", "code", NULL };
static const char *runmenucmd[] = { "dmenu_run", NULL };
static const char *sshmenucmd[] = { "dmenu_ssh", NULL };
static const char *winmenucmd[] = { "dmenu_win", NULL };
static const char *drunmenucmd[] = { "dmenu_drun", NULL };
static const char *exitmenucmd[] = { "dmenu_exit", NULL };
static const char *slockcmd[] = { "slock", NULL };
static const char *duptermcmd[] = { "dupterm", NULL };

static const Key keys[] = {
	/* modifier                     key        function        argument */
	{ MODKEY,                       XK_c,      spawn,          {.v = termcmd } },
	{ MODKEY|Mod1Mask,              XK_c,      spawn,          {.v = pulltermcmd } },
	{ MODKEY,                       XK_e,      spawn,          {.v = emacscmd } },
	{ MODKEY|Mod1Mask,              XK_e,      spawn,          {.v = pullemacscmd } },
	{ MODKEY|Mod1Mask,              XK_g,      spawn,          {.v = browsercmd } },
	{ MODKEY|Mod1Mask,              XK_u,      spawn,          {.v = codecmd } },
	{ MODKEY,                       XK_r,      spawn,          {.v = runmenucmd } },
	{ MODKEY|Mod1Mask,              XK_r,      spawn,          {.v = runmenucmd } },
	{ MODKEY,                       XK_s,      spawn,          {.v = sshmenucmd } },
	{ MODKEY|Mod1Mask,              XK_s,      spawn,          {.v = sshmenucmd } },
	{ MODKEY,                       XK_w,      spawn,          {.v = winmenucmd } },
	{ MODKEY|Mod1Mask,              XK_w,      spawn,          {.v = winmenucmd } },
	{ MODKEY,                       XK_p,      spawn,          {.v = drunmenucmd } },
	{ MODKEY|Mod1Mask,              XK_p,      spawn,          {.v = drunmenucmd } },
	{ MODKEY,                       XK_q,      spawn,          {.v = exitmenucmd } },
	{ MODKEY,                       XK_z,      spawn,          {.v = slockcmd } },
	{ MODKEY|ShiftMask,             XK_Return, spawn,          {.v = duptermcmd } },
	{ MODKEY,                       XK_b,      togglebar,      {0} },
	{ MODKEY|ShiftMask,             XK_b,      togglebar,      {.ui = 1} },
	{ MODKEY|ShiftMask,             XK_j,      rotatestack,    {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_k,      rotatestack,    {.i = -1 } },
	{ MODKEY,                       XK_j,      focusstack,     {.i = +1 } },
	{ MODKEY,                       XK_k,      focusstack,     {.i = -1 } },
	{ MODKEY,                       XK_i,      incnmaster,     {.i = +1 } },
	{ MODKEY,                       XK_d,      incnmaster,     {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_i,      resetnmaster,   {.ui = nmaxmaster } },
	{ MODKEY|ShiftMask,             XK_d,      resetnmaster,   {.ui = nminmaster} },
	{ MODKEY,                       XK_h,      setmfact,       {.f = -0.05} },
	{ MODKEY,                       XK_l,      setmfact,       {.f = +0.05} },
	{ MODKEY|ShiftMask,             XK_h,      setsmfact,      {.f = +0.05} },
	{ MODKEY|ShiftMask,             XK_l,      setsmfact,      {.f = -0.05} },
	{ MODKEY,                       XK_Return, zoom,           {0} },
	{ MODKEY,                       XK_Tab,    view,           {0} },
	{ MODKEY|ShiftMask,             XK_c,      killclient,     {0} },
	{ MODKEY|ControlMask,           XK_c,      killclient,     {.ui = 1} },  // kill unselect
	{ MODKEY|ShiftMask|ControlMask, XK_c,      killclient,     {.ui = 2} },  // killall
	{ MODKEY,                       XK_t,      setlayout,      {.v = &layouts[0]} },
	{ MODKEY,                       XK_f,      setlayout,      {.v = &layouts[1]} },
	{ MODKEY,                       XK_m,      setlayout,      {.v = &layouts[2]} },
	{ MODKEY,                       XK_g,      setlayout,      {.v = &layouts[3]} },
	{ MODKEY,                       XK_space,  setlayout,      {0} },
	{ MODKEY|ShiftMask,             XK_space,  togglefloating, {0} },
	{ MODKEY,                       XK_o,      setlayout,      {0} },
	{ MODKEY|ShiftMask,             XK_o,      togglefloating, {0} },
	{ MODKEY|ShiftMask,             XK_f,      togglefullscr,  {0} },
	{ MODKEY|ShiftMask,             XK_s,      togglesticky,   {0} },
	{ MODKEY,                       XK_u,      swapfocus,      {0} },
	{ MODKEY,                       XK_0,      view,           {.ui = ~0 } },
	{ MODKEY|ShiftMask,             XK_0,      tag,            {.ui = ~0 } },
	{ MODKEY,                       XK_comma,  focusmon,       {.i = -1 } },
	{ MODKEY,                       XK_period, focusmon,       {.i = +1 } },
	{ MODKEY|ShiftMask,             XK_comma,  tagmon,         {.i = -1 } },
	{ MODKEY|ShiftMask,             XK_period, tagmon,         {.i = +1 } },
	{ MODKEY,                       XK_Left,   viewtoleft,     {0} },
	{ MODKEY,                       XK_Right,  viewtoright,    {0} },
	{ MODKEY|ShiftMask,             XK_Left,   tagtoleft,      {0} },
	{ MODKEY|ShiftMask,             XK_Right,  tagtoright,     {0} },
	{ MODKEY|ControlMask,           XK_Left,   rotatetags,     {.i = -1 } },
	{ MODKEY|ControlMask,           XK_Right,  rotatetags,     {.i = +1 } },
	{ MODKEY,                       XK_minus,  setgaps,        {.i = -5 } },
	{ MODKEY,                       XK_equal,  setgaps,        {.i = +5 } },
	{ MODKEY|ShiftMask,             XK_minus,  setgaps,        {.i = GAP_RESET } },
	{ MODKEY|ShiftMask,             XK_equal,  setgaps,        {.i = GAP_TOGGLE} },
	{ MODKEY|ControlMask,           XK_h,      floathorimax,   {0} },
	{ MODKEY|ControlMask,           XK_l,      floathorimax,   {0} },
	{ MODKEY|ControlMask,           XK_j,      floatvertmax,   {0} },
	{ MODKEY|ControlMask,           XK_k,      floatvertmax,   {0} },
	{ MODKEY|ControlMask,           XK_m,      floatmaximize,  {0} },
	TAGKEYS(                        XK_1,                      0)
	TAGKEYS(                        XK_2,                      1)
	TAGKEYS(                        XK_3,                      2)
	TAGKEYS(                        XK_4,                      3)
	TAGKEYS(                        XK_5,                      4)
	TAGKEYS(                        XK_6,                      5)
	TAGKEYS(                        XK_7,                      6)
	TAGKEYS(                        XK_8,                      7)
	TAGKEYS(                        XK_9,                      8)
	{ MODKEY|ShiftMask,             XK_q,      quit,           {0} },
	{ MODKEY|ControlMask|ShiftMask, XK_q,      quit,           {1} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};
