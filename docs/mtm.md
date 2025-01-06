mtm
===

## URLs

```
## https://github.com/deadpixi/mtm
```

## Build mtm -static


```bash
## Build static in Alpine
## https://pkgs.alpinelinux.org/packages
$ apk add build-base
$ apk search ncurses
$ apk add ncurses-dev
$ apk info -qL ncurses-dev
$ git clone https://github.com/deadpixi/mtm
$ cd mtm
$ make HEADERS='-DNCURSESW_INCLUDE_H="<ncurses.h>"'
```

## Usage guide

```
## The -T flag tells mtm to assume a different kind of host terminal
## The -t flag tells mtm what terminal type to advertise itself as
##   Note that this doesn't change how mtm interprets control sequences
##   it simply controls what the TERM environment variable is set to
## The -c flag lets you specify a keyboard character to use as the "command prefix"
##   By default, this is g.
$ mtm [-T NAME] [-t NAME] [-c KEY]

## By the command prefix (by default ctrl-g)
# Up/Down/Left/Right Arrow
# o
# h/v
# w
# l
# PgUp/PgDown/End
```
