---
name: microemacs-test
description: Build and test MicroEMACS Linux me and Windows me.exe binaries. Use when validating MicroEMACS changes, running its test suite, checking version, color, Lisp, minimal builds, or multipage behavior, building with MinGW, or testing me.exe through Wine.
---

# MicroEMACS Testing

## Overview

Validate the Linux `me` and Windows `me.exe` builds using the repository test suite, Docker-based MinGW build, and Wine smoke test.

## Commit Convention

Use the `ME: ` prefix for MicroEMACS commit subjects, for example:
`ME: add Lisp editor primitives`.

## Workflow

### 1. Run source tests

From the repository root:

```sh
git status --short
cd tools/MicroEMACS
./build.sh -b test clean all
./test/syntax-test
./test/display-test
./test/lisp-test
```

Run `./test/cscope-test` only when validating cscope changes and provide any fixture or arguments its test expects.

### 2. Validate Linux me

For a full static-binary validation, still from `tools/MicroEMACS`:

```sh
./build.sh -p
./me --version
```

Use interactive checks only when the user asks for manual testing:

- `./me` — normal editor startup.
- `./me -C` — color disabled.
- `./me -2`, `./me -3`, or `./me -4` — multipage display.

### 3. Validate Linux minimal me

From `tools/MicroEMACS`, build the minimal editor without color and Lisp:

```sh
./build.sh -m
./me --version
```

Confirm that the usage string omits both `C` and `[-l lisp]`, and that the
binary does not contain the `eval-lisp` command:

```sh
if strings ./me | grep -q 'eval-lisp'; then exit 1; fi
```

For a stripped static minimal build:

```sh
make -f Make.Linux clean
make -f Make.Linux STATIC=yes COLOR=no LISP=no me strip
file ./me
./me --version
```

If a final standard static artifact is required afterward, restore it with:

```sh
./build.sh -p
```

### 4. Build and validate Windows me.exe

From `tools/MicroEMACS`, build the Windows binary:

```sh
./build.sh -w
```

From the repository root, run the headless Wine smoke test:

```sh
./docker/run.sh wine
```

For an interactive Windows-console test, use:

```sh
./docker/run.sh wine gui
```

The GUI mode requires a working host `DISPLAY`. Add editor arguments after `gui`, for example `./docker/run.sh wine gui -2`.

Use `./docker/run.sh wine shell` to inspect the Wine environment.

For a minimal Windows build without color and Lisp:

```sh
./build.sh -w COLOR=no LISP=no
```

Then run the same headless Wine smoke test from the repository root.

### 5. Handle missing Docker images

Build the required image from the repository root:

```sh
make -C docker/dockerfiles/tool mingw-base
make -C docker/dockerfiles/tool wine-base
```

## Guardrails

- Do not use Wine's `--backend=curses`; Ubuntu Wine 6 does not support it reliably.
- Confirm both `./me --version` and the Wine smoke test report the same MicroEMACS version.
- Confirm minimal builds omit `C`, `[-l lisp]`, and `eval-lisp`; standard builds include them.
- Check `git diff --check` before reporting validation results.
- Do not commit test binaries or generated archives unless the user explicitly asks.
