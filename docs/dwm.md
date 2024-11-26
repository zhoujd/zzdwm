DWM
===

## URLs

    ## https://dwm.suckless.org/
    ## https://dwm.suckless.org/patches/
    ## https://dwm.suckless.org/tutorial/
    ## https://github.com/bakkeby/patches
    ## https://github.com/bakkeby/dwm-flexipatch
    ## https://wiki.archlinuxcn.org/wiki/Dwm

## Relative Source

    ## Fetch source code
    $ git clone https://git.suckless.org/dwm
    $ git clone https://git.suckless.org/dmenu
    $ git clone https://git.suckless.org/st

## Dave's Visual Guide to dwm

    ## https://ratfactor.com/dwm
    ## https://racfactor.com/dwm2
    ## https://ratfactor.com/slackware/three-monitors

## Status Monitor

    ## https://dwm.suckless.org/status_monitor/

## Key Chain

    ## https://dwm.suckless.org/patches/keychain/

## dwm-win32

    ## https://github.com/prabirshrestha/dwm-win32

## Controlling Audio through keys in dwm

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


## Switch to Application

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

## Font monospace

    ## https://www.freedesktop.org/wiki/Software/fontconfig/
    ## https://fontconfig.pages.freedesktop.org/fontconfig/fontconfig-user.html
    $ fc-match monospace
    DejaVuSansMono.ttf: "DejaVu Sans Mono" "Book"
    $ fc-match monospace:bold
    DejaVuSansMono-Bold.ttf: "DejaVu Sans Mono" "Bold"

## Font Unicode

    ## https://fontawesome.com/search
    ## https://fontawesome.com/v4/cheatsheet/

## Launch or Focus

    ## https://github.com/octetz/linux-desktop/blob/master/s/lof
    ## https://github.com/joshrosso/linux-desktop/blob/master/s/lof_chromium
    ## https://github.com/mkropat/jumpapp
