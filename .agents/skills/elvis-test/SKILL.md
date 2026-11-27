---
name: elvis-test
description: Build and test the elvis vi clone in tools/editor/elvis. Use when validating elvis changes, building or publishing through its build.sh and the docker alpine image, patching elvis sources or editor defaults, checking the git version stamp, editing doc/elvis.9, or committing elvis changes.
---

# elvis Testing

## Overview

Validate the `tools/editor/elvis` tree: Elvis 2.2.2, Steve
Kirkendall's vi clone, built from upstream's `./configure` +
`make` with a custom patch applied on top — default `showmode`
on and ruler off (`vi.c`), the `^G` message (`exaction.c`), `bs`
(`optglob.c`), the `safeCHARdup` NULL guard for the alpine crash
(`safe.c`), the linux/alpine build fixes (`guitcap.c`,
`osunix/osprg.c`, `configure` prefix), version stamping (`ver.sh`,
`Makefile.in`, `main.c`), and `build.sh`.

## Commit Convention

Use the `elvis: ` prefix for elvis commit subjects, matching the
history style `elvis: update <file> on <reason>` and
`elvis: add <thing>`.

## Workflow

### 1. Build

From `tools/editor/elvis`:

```sh
./build.sh -b      # ./configure --without-x linux, make -O0 -g
./build.sh -r      # release: -Os -s, static on alpine/void hosts
./build.sh -p      # publish: docker zhoujd/alpine static, then upx
./build.sh -c      # git clean -dfx: full reset of the build tree
./build.sh -i      # make install (sudo when not root)
```

The build produces `elvis`, `ref`, `ctags`, `elvtags`, `fmt`, and
`elvfmt` — ctags/elvtags and fmt/elvfmt are the same program under
different names.  Publish upxes `elvis`, `fmt`, `ctags` on the
host after the docker build succeeds.

`clean` is `git clean -dfx`: it removes the generated Makefile,
`rev.h`, and every binary.  `config.h` survives only because it
is tracked — treat edits to it as source changes to commit, and
put any new generated file into `.gitignore` so clean resets it.

### 2. Version stamping

`ver.sh` writes `rev.h` (`DATE` plus `git-<7>` from HEAD),
`Makefile.in` has `main.o: $(REV)`, and `--version` reports
`elvis 2.2.2 <date> git-<rev>`.  The REV names HEAD at build
time, so commit the source change first and run `./build.sh -p`
after it — a build before the commit stamps the old code.

### 3. Verify

There is no automated suite.  Verify with the build, `--version`,
and `./e2` — upstream's wrapper that runs the uninstalled
`./elvis` with `ELVISPATH=data:doc:~/.elvis`; `./e2 ddd` runs it
under the debugger.  Upstream's `make every` builds `verify`, a
component test harness, but it `#error`s against the tracked
`config.h`'s full feature set — not usable as-is.

### 4. Documentation

`doc/elvis.9.man` and `doc/elvis.9.html` are the project's own
short man page, written against the `mnx.mac` macros — keep them
in step with behavior changes such as defaults.

## Guardrails

- `publish` requires the `zhoujd/alpine` image and host `upx`;
  the docker run propagates failure with `|| exit 1` and upx
  runs only after a successful build, so a failure never
  compresses stale binaries.
- Rebuild with `./build.sh -p` after committing when the binaries
  will be deployed, so the REV stamp names the code they contain.
- Binary artifacts are gitignored — commit sources only.
- Check `git diff --check` before reporting validation results.
