Utils
=====

## URLs

```
## https://github.com/ericpruitt/edge
## https://github.com/altsem/gitu
```

## Blackwalls

```
## https://github.com/ericpruitt/edge/blob/master/utilities/blackwalls.c
```

## Micro Terminal Multiplexer

```
## https://github.com/deadpixi/mtm
## https://pkgs.alpinelinux.org/packages
## Build static in Alpine
$ apk add build-base
$ apk search ncurses
$ apk add ncurses-dev
$ apk info -qL ncurses-dev
$ git clone https://github.com/deadpixi/mtm
$ cd mtm
$ make HEADERS='-DNCURSESW_INCLUDE_H="<ncurses.h>"'
```

## Detach feature of screen

```
##
$ git clone https://github.com/crigler/dtach
$ cd dtach
$ configure
$ make LDFLAGS=-static
$ ./detach --help
```
