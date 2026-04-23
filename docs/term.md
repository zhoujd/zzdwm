Term
====

## URLs

```
## https://github.com/xterm-x11/xterm.dev
## https://github.com/xterm-x11/xterm.dev/blob/main/xterm-on-invisible-island/terminfo.src.gz
## https://github.com/alexander-naumov/gnu-screen
## https://github.com/alexander-naumov/gnu-screen/blob/main/src/terminfo/screeninfo.src
```

## Decompile a Binary Terminfo to Source

```
$ infocmp
$ infocmp xterm-256color > xterm-256color.ti
$ infocmp screen-256color > screen-256color.ti
$ infocmp -A /path/to/binary/terminfo > source.ti
```

## Compile a source file

```
## By default, this installs the compiled entry to
## ~/.terminfo/ for the current user
## or /usr/share/terminfo/ if run as root.
$ tic source.ti
```
