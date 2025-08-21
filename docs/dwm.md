DWM
===

## URLs

```
## https://dwm.suckless.org/
## https://dwm.suckless.org/patches/
## https://dwm.suckless.org/tutorial/
## https://suckless.org/rocks/
## https://github.com/bakkeby/patches
## https://github.com/bakkeby/dwm-flexipatch
## https://wiki.archlinuxcn.org/wiki/Dwm
## https://github.com/ericpruitt/edge
## https://github.com/Tigermouthbear/dwm
```

## Relative Source

```bash
## Fetch source code
$ git clone https://git.suckless.org/dwm
$ git clone https://git.suckless.org/dmenu
$ git clone https://git.suckless.org/st
```

## Dave's Visual Guide to dwm

```
## https://ratfactor.com/dwm
## https://racfactor.com/dwm2
## https://ratfactor.com/slackware/three-monitors
```

## Status Monitor

```
## https://dwm.suckless.org/status_monitor/
```

## Key Chain

```
## https://dwm.suckless.org/patches/keychain/
```

## dwm-win32

```
## https://github.com/prabirshrestha/dwm-win32
```

## Controlling Audio through keys in dwm

```c
/* Volume Commands */
static const char *volume[3][4] = {
  {"pactl", "set-sink-volume", "@DEFAULT_SINK@", "+10%"},
  {"pactl", "set-sink-volume", "@DEFAULT_SINK@", "-10%"},
  {"pactl", "set-sink-mute", "@DEFAULT_SINK@", "toggle"},
};

/* Keys */
static Key keys[] = {
/* modifier key                     function  argument */
  {0,       XF86XK_AudioRaiseVolume, spawn,   {.v=volume[0]} },
  {0,       XF86XK_AudioLowerVolume, spawn,   {.v=volume[1]} },
  {0,       XF86XK_AudioMute,        spawn,   {.v=volume[2]} },
  // The rest of your keys and config file......
}

// https://cgit.freedesktop.org/xorg/proto/x11proto/tree/XF86keysym.h
#include <X11/XF86keysym.h>
```

## Switch to Application

```bash
## With xdotool
$ application=emacs
$ xdotool search ".*${application}.*" windowactivate
$ xdotool search emacs windowactivate

## With wmctrl
$ sudo apt install wmctrl
$ wmctrl -a emacs

## With xdo
$ sudo apt install xdo
$ xdo activate -N firefox || firefox
$ xdo activate -N Emacs || emacs
```

## Font monospace

```bash
## https://www.freedesktop.org/wiki/Software/fontconfig/
## https://fontconfig.pages.freedesktop.org/fontconfig/fontconfig-user.html
## https://unix.stackexchange.com/questions/106070/changing-monospace-fonts-system-wide
## Or to /etc/fonts/local.conf to set it system-wide
$ cat ~/.config/fontconfig/fonts.conf <<EOF
<match target="pattern">
  <test name="family" qual="any">
    <string>monospace</string>
  </test>
  <edit binding="strong" mode="prepend" name="family">
    <string>SF Mono</string>
  </edit>
</match>
EOF

$ fc-match monospace
SF-Mono-Regular.otf: "SF Mono" "Regular"
$ fc-match monospace:bold
SF-Mono-Bold.otf: "SF Mono" "Bold"
```

## Getting antialiased fonts in dwm

```
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
    <!-- Enable antialiasing for all fonts -->
    <match target="font">
        <edit mode="assign" name="antialias"><bool>true</bool></edit>
    </match>
    <!-- Enable hinting -->
    <match target="font">
        <edit name="hinting" mode="assign"><bool>true</bool></edit>
    </match>
    <match target="font">
        <edit name="hintstyle" mode="assign"><const>hintslight</const></edit>
    </match>
    <!-- subpixel rendering -->
    <match target="font">
        <edit name="rgba" mode="assign"><const>rgb</const></edit>
    </match>
</fontconfig>
```

## Font Unicode

```
## https://fontawesome.com/search
## https://fontawesome.com/v4/cheatsheet
```

## Launch or Focus

```
## https://github.com/octetz/linux-desktop/blob/master/s/lof
## https://github.com/joshrosso/linux-desktop/blob/master/s/lof_chromium
## https://github.com/mkropat/jumpapp
```

## Color Palette

```
## https://www.color-hex.com/
## https://www.color-hex.com/color-palettes/
## https://www.color-hex.com/color-palette/1034689
```

## Message Copilot (AI Chat)

```
## https://copilot.microsoft.com/
## https://copilot.microsoft.com/chats/Tg9aNkP1PwPC9kHydpxMF
```

## Debug DWM

```bash
## Xephyr is a nested X server that runs as an X application
$ sudo apt install xserver-xephyr

## Adding the -g in CFLAGS in config.mk
## CFLAGS = -g
$ Xephyr -br -ac -noreset -screen 800x600 :2 &
$ export DISPLAY=:2
$ make
$ gdb ./dwm
```

## DWM dracula Theme

```c
// https://github.com/dracula/dwm/blob/main/config.h
static const char col_gray1[]       = "#282a36";
static const char col_gray2[]       = "#ffb86c";
static const char col_gray3[]       = "#ff79c6";
static const char col_gray4[]       = "#ffb86c";
static const char col_cyan[]        = "#282a36";
static const char *colors[][3]      = {
    /*               fg         bg         border   */
    [SchemeNorm] = { col_gray3, col_gray1, col_cyan },
    [SchemeSel]  = { col_gray4, col_cyan,  col_gray2 },
};
```

## GTK theming

```bash
## Theme: WhiteSur-Dark
$ wget https://github.com/vinceliuice/WhiteSur-gtk-theme/archive/refs/heads/master.zip
## Icon: WhiteSur-Dark
$ wget https://github.com/vinceliuice/WhiteSur-icon-theme/archive/refs/heads/master.zip

## Cursor: WhiteSur-cursors
$ wget https://github.com/vinceliuice/WhiteSur-cursors/archive/refs/heads/master.zip
$ unzip master.zip
$ rm -rf ~/.icons/WhiteSur-cursors
$ cp -r WhiteSur-cursors-master/dist ~/.icons/WhiteSur-cursors
$ rm -r WhiteSur-cursors-master master.zip

## Default cursor
$ mkdir -p ~/.icons/default
$ cat ~/.icons/default/index.theme <<EOF
[Icon Theme]
Name=Default
Comment=Default Cursor Theme
Inherits=WhiteSur-cursors
EOF

## Change icon and cursor theme
$ sudo apt install lxappearance
$ lxappearance
```

## The command-not-found.com

```
https://command-not-found.com/
https://command-not-found.com/xprop
```

## DWM GruvBox Theme

```c
/* https://github.com/plasmoduck/themes/tree/master/gruvbox */
/* colors */
static const char norm_fg[]     = "#a89984";
static const char norm_bg[]     = "#282828";
static const char norm_border[] = "#928374";
static const char sel_fg[]      = "#282828";
static const char sel_bg[]      = "#d65d0e";
static const char sel_border[]  = "#a89984";

static const char urg_fg[]      = "#a89984";
static const char urg_bg[]      = "#cc241d";
static const char urg_border[]  = "#b8bb26";
static const char title_fg[]    = "#b8bb26";
static const char title_bg[]    = "#3A3A3A";

static const char col_borderbar[] = "#75715e";

static const char *colors[][3]  = {
    /*               fg            bg         border                         */
    [SchemeNorm]  = { norm_fg,     norm_bg,   norm_border }, // unfocused wins
    [SchemeSel]   = { sel_fg,      sel_bg,    sel_border },  // the focused win
    [SchemeUrg]   = { urg_fg,      urg_bg,    urg_border },
    [SchemeTitle] = { title_fg,    title_bg,  norm_border },
};
```
## DWM Default Theme

```
/* colors */
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char col_cyan[]        = "#005577";
static const char *colors[][3]      = {
    /*                fg         bg         border   */
    [SchemeNorm]  = { col_gray3, col_gray1, col_gray2 },
    [SchemeSel]   = { col_gray4, col_cyan,  col_cyan  },
    [SchemeTitle] = { col_gray4, col_cyan,  col_cyan  },
};
```

## _NET_WM_DESKTOP and _NET_ACTIVE_WINDOW

```
## https://github.com/ericpruitt/mydwm/blob/master/patches/10-more-ewmhs.diff
_NET_WM_DESKTOP
    Query and set what desktop a window is living in.
    Support for this enables these commands: "set_desktop_for_window", "get_desktop_for_window".

_NET_ACTIVE_WINDOW
    Allows you to query and set the active window by asking the window manager to bring it forward.
    Support for this enables these commands: "windowactivate", "getactivewindow".
```

## Extended Window Manager Hints (EWMH) spec

```
## Latest spec
## https://specifications.freedesktop.org/wm-spec/latest/

## 3.6 _NET_CURRENT_DESKTOP
## https://specifications.freedesktop.org/wm-spec/latest/ar01s03.html#id-1.4.8

## The ewmh python module
## https://ewmh.readthedocs.io/en/latest/ewmh.html
```
