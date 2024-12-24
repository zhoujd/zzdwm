DWM
===

## URLs

```
## https://dwm.suckless.org/
## https://dwm.suckless.org/patches/
## https://dwm.suckless.org/tutorial/
## https://github.com/bakkeby/patches
## https://github.com/bakkeby/dwm-flexipatch
## https://wiki.archlinuxcn.org/wiki/Dwm
## https://github.com/ericpruitt/edge
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
EOF
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
    [SchemeSel]  = { col_gray4, col_cyan,  col_gray2  },
};
```
