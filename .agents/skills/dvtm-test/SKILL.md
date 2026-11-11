---
name: dvtm-test
description: Build dvtm and run its pty end-to-end tests. Use when validating dvtm changes, window borders and layouts, the status bar, key bindings, mouse relay and the (M)/(m) mouse indicators, or when driving dvtm in a pseudo-terminal with injected keys and mouse reports.
---

# dvtm Testing

## Overview

Validate the `tools/dvtm` build: the C changes through `make` and the
pty end-to-end scripts, which drive the real `./dvtm` binary in a
pseudo-terminal with injected keys and mouse reports.

## Commit Convention

Use the `dvtm: ` prefix for dvtm commit subjects, for example:
`dvtm: relay mouse reporting with content coordinates`.

## Workflow

### 1. Build

From `tools/dvtm`:

```sh
make
```

A config change (`config.h`, `config.def.h`) needs a rebuild to take;
`make` recompiles on the touched headers.

### 2. Know what testsuite.sh is

`testsuite.sh` is upstream's interactive script: it prints key sequences
for a manually started dvtm and assumes `vis` and upstream's `CTRL+g`
MOD key, so it does not drive this fork as-is. The pty scripts below are
the automated suite; run `testsuite.sh` only when hand-testing upstream
behavior.

### 3. Run the pty end-to-end scripts

The Python pty tests live in `test/python/`. Each is self-contained and
skips when a needed binary is absent:

```sh
for s in test/python/*.py; do python3 "$s" || break; done
```

`mouse_indicator.py` also needs a built MicroEMACS `me` in
`../MicroEMACS` (the `me_dvtm_e2e.py` script in MicroEMACS's
`test/python/` covers the relay the same way, from the me side).

### 4. Drive dvtm by hand in a pty

The harness conventions the scripts share:

- `TERM=st-256color` and `LANG=en_US.UTF-8` in the child; the
  `st-256color` terminfo entry must be installed.
- `TIOCSWINSZ` the pty after `fork` — dvtm sizes its windows from it.
- `MOD` is `CTRL('\`)`, byte `0x1c`; two-key bindings are one write,
  for example `MOD + b"I"` for `toggleinfo`.
- Mouse reports are injected where the outer terminal would send them:
  X10 as `\e[M` with 32-based bytes, SGR as `\e[<...M/m`.
- Read with a drain loop around `select`, assert on byte fragments —
  ncurses writes only the changed cells, so a redraw may stream a
  fragment of a line, not the whole line. To assert on a whole line,
  send `MOD+CTRL('L')` first: dvtm's `redraw` ends in
  `clearok(curscr, TRUE)`, forcing a complete repaint.
- `SIGTERM` the child and `waitpid` in a `finally`.

### 5. Install the terminfo description when needed

```sh
tic -s dvtm.info
```

Running dvtm itself under `DVTM_TERM` or without its terminfo shows the
"terminal is not fully functional" warning the README describes.

## Guardrails

- `taglayouts[]` in `config.h` starts tag 1 in the fullscreen layout —
  a fresh dvtm is not in tile, so switch with `MOD+t` before asserting
  on a layout symbol.
- The border's layout symbols include `[M]` (fullscreen); new border
  markers must not collide with them.
- `config.h` is the live config, `config.def.h` the template — keep
  both in step for anything meant as a default.
- Check `git diff --check` before reporting validation results.
