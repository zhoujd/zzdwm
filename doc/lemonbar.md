lemonbar
========

## URLs

```
## https://wiki.archlinux.org/title/Lemonbar
## https://github.com/LemonBoy/bar
```

## Build

```
$ sudo apt update -y
$ sudo apt install -y libx11-xcb-dev libxcb-randr0-dev libxcb-xinerama0-dev
$ CFLAGS='-DWITH_XINERAMA=1' make
$ ./lemonbar -h || exit 0
```

## XFT Support

```
https://github.com/silentz/lemonbar-xft
```
