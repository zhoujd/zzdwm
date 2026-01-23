bspwm
=====

## URLs

```
## https://www.instructables.com/Bspwm-Installation-and-Configuration/
## https://wiki.archlinux.org/title/Comparison_of_tiling_window_managers
```

## Build

```
$ sudo apt install apt-file
$ apt-file search xcb/xcb_keysyms.h
$ apt-file search xcb/shape.h

$ sudo apt install libxcb-icccm4-dev libxcb-shape0-dev

$ git clone https://github.com/baskerville/bspwm
$ cd bspwm
$ make

$ git clone https://github.com/baskerville/sxhkd
$ cd sxhkd
$ make
```
