---
name: vi-test
description: Build and test the busybox vi port in tools/editor/vi. Use when validating vi changes, running its pty e2e suite, building the static musl publish binary through docker alpine, adjusting the bb.h feature set or libbb shims, or committing vi changes.
---

# vi Testing

## Overview

Validate the `tools/editor/vi` port: busybox's `editors/vi.c` as a
single Linux application — the C source is upstream's, unchanged but
for the `#include "bb.h"` and `main()` in place of `vi_main()`.
`bb.h` carries the sliver of libbb vi.c leans on and the feature
configuration.

## Commit Convention

Use the `vi: ` prefix for vi commit subjects, for example:
`vi: add build.sh with the static publish build`.

## Workflow

### 1. Build

From `tools/editor/vi`:

```sh
./build.sh -b          # make: the quick glibc build the tests run
./build.sh -b test     # build, then the pty e2e suite (-t works too)
./build.sh -d          # clean, then a DEBUG=yes -g -O0 build
./build.sh -c          # make clean
```

The local build shows ~18 `-Wsign-compare` warnings from the
untouched upstream vi.c; they are benign — do not chase them.

### 2. Publish the static binary

```sh
./build.sh -p          # docker zhoujd/alpine (musl): STATIC=yes strip
file vi                # must report: statically linked, stripped
```

`-p` replaces the in-tree `vi` with the musl binary, so rerun the
e2e suite after it to validate that binary, and rebuild with
`./build.sh -b` when the glibc build is wanted back. `--upx`/`-u`
compresses after the docker build. The `vi` binary is gitignored:
commit only sources.

### 3. Run the pty end-to-end suite

`test/vi_e2e.py` drives the real `./vi` in a pseudo-terminal: insert
with `:wq`, `dd` with undo and `:q!`, `$`/`x` motions, and `-R`
refusing the write. The harness conventions to keep when extending
it:

- `TIOCSWINSZ` the pty to 24x80 right after `pty.fork`.
- The `run()` helper accumulates every drain window's output —
  status-line messages land in intermediate windows, not the last,
  so assert on the accumulated bytes.
- A hung child is killed and reaped, and `run()` returns a `None`
  status so the check fails instead of blocking the suite.

### 4. The bb.h contract

All adaptation lives in `bb.h` — vi.c stays upstream:

- The feature set is the `ENABLE_FEATURE_VI_*` block; `CRASHME`
  stays off (vi.c itself forces it) and `REGEX_SEARCH` stays off:
  its code needs the GNU regex API (`re_syntax_options`, `re_search`,
  `REG_STARTEND`) that glibc has and musl has not, and busybox
  itself defaults the feature off for exactly that reason. Search
  and `:s` run the literal-match path, with `ignorecase` honored.
- `bb.h` must not define `ESC` or `write1` — vi.c owns both.
- The numeric knobs are `CONFIG_FEATURE_VI_MAX_LEN` 4096 and
  `CONFIG_FEATURE_VI_UNDO_QUEUE_MAX` 256.

## Guardrails

- vi.c keeps exactly the two adaptations (the include, `main()`);
  behavioral fixes belong in `bb.h`.
- Do not turn `REGEX_SEARCH` back on: the docker publish build
  breaks on musl, and the local and publish binaries would diverge.
- Check `git diff --check` before reporting validation results.
