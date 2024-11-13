DWM
===

## URLs

    ## https://dwm.suckless.org/
    ## https://dwm.suckless.org/patches/
    ## https://dwm.suckless.org/tutorial/
    ## https://github.com/bakkeby/patches
    ## https://github.com/bakkeby/dwm-flexipatch

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
    $ wmctrl -a emacs

## Font monospace

    ## https://www.freedesktop.org/wiki/Software/fontconfig/
    ## https://fontconfig.pages.freedesktop.org/fontconfig/fontconfig-user.html
    $ fc-match monospace
    DejaVuSansMono.ttf: "DejaVu Sans Mono" "Book"
    $ fc-match monospace:bold
    DejaVuSansMono-Bold.ttf: "DejaVu Sans Mono" "Bold"
