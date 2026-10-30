# zzdwm

A self-contained, dwm-centric Linux desktop workspace: window manager sources
(dwm, cwm), a heavily extended MicroEMACS editor, suckless companion tools,
terminal multiplexers, AI helpers, and Docker dev environments — all built and
installed by a single `init.sh` bootstrap script.

Per-tool notes live in `doc/`.

## Build dependence

```sh
$ sudo apt install make gcc g++ yacc
$ sudo apt install libx11-dev libxft-dev libxinerama-dev libxrender-dev
$ sudo apt install libasound2-dev
$ sudo apt install libxfixes-dev libxi-dev libxext-dev
$ sudo apt install libxrandr-dev
$ sudo apt install libxcomposite-dev libxdamage-dev
$ sudo apt install libx11-xcb-dev libxcb-randr0-dev libxcb-xinerama0-dev
$ sudo apt install libcairo2-dev libxdo-dev
```

Void Linux is also supported (`sudo xbps-install ...`); see `install_dep` in
`init.sh` for the package list. Or simply run `./init.sh dep`.

## Init dwm

```sh
$ ./init.sh          # show usage
$ ./init.sh dep      # install system dependencies
$ ./init.sh install  # install all components (dm, bin, misc, tool)
$ ./init.sh build    # build dwm, cwm, dmenu, dwmstatus, slock, shotkey,
                     # x11, xbanish, xcompmgr, then clean artifacts
$ ./init.sh all      # dep + install + build + clean
```

Install prefix for binaries defaults to `/usr/local` and can be overridden
with `DEST`:

```sh
$ DEST=~/.local ./init.sh install
```

## URLs

```txt
- https://dwm.suckless.org/
- https://dl.suckless.org/dwm/
- https://git.suckless.org/dwm/
- https://git.suckless.org/dwm/files.html
```
