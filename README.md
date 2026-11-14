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

## AI agent

The workspace includes a compact, static OpenAI-compatible agent with
interactive approval, one-shot execution, file/stdin context, workspace
scoping, skills, and live steering.

```sh
$ make -C tools/ai/agent ai
$ tools/ai/agent/ai                          # interactive approval mode
$ tools/ai/agent/ai tui                      # three-pane ncurses interface
$ tools/ai/agent/ai -f main.c -e "review this code"
$ printf 'build output\n' | tools/ai/agent/ai -e "find the failure"
```

The TUI separates AI/user thoughts, command activity/output, and prompt input.
Set `AI_API_URL`, `AI_MODEL`, and optionally `AI_API_KEY` or `OPENAI_API_KEY`;
set `NO_COLOR=1` to disable structural colors. See
[tools/ai/agent/README.md](tools/ai/agent/README.md) for one-shot exit codes,
security boundaries, configuration, skills, and live steering.

## URLs

```txt
- https://dwm.suckless.org/
- https://dl.suckless.org/dwm/
- https://git.suckless.org/dwm/
- https://git.suckless.org/dwm/files.html
```
