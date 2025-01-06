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
