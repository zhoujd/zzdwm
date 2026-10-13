# MicroEMACS

MicroEMACS is a compact Emacs-style editor derived from Dave Conroy's
1986 public-domain version. This tree modernizes that editor for Linux
and Windows while keeping the executable small and fast.

## Features

* UTF-8 editing and Unicode-aware display.
* Regular-expression search and replacement.
* Undo, keyboard macros, profiles, and buffer/window management.
* Optional color and Vim-style line numbers.
* Syntax highlighting for C, C++, Bash, Python, Markdown, JSON, YAML,
  TOML, CMake, Makefile, Dockerfile, Lisp, Emacs Lisp, Scheme, and SQL.
* Tiny Lisp extensions, editor command bridges, Lisp startup configuration,
  and configurable mode-line colors.
* Tags, cscope, grep, GCC error navigation, and ispell integration.
* Linux terminal support through ncursesw or termcap.
* Native Windows console support built with MinGW-w64.

Syntax highlighting detects file types from extensions and shebang
lines. Language modes can also be selected explicitly with commands
such as `M-X c-mode`, `M-X python-mode`, and `M-X sql-mode`. Only the
visible portion of each buffer is rendered, and syntax state is cached
while scrolling.

## Quick Start

From this directory, build and run the editor:

```sh
./build.sh -b
./me
```

Useful startup options include:

```sh
./me file.c:123:4    # Open a file at line 123, column 4
./me -C file.c       # Disable color for this session
./me -n file.c       # Show line numbers
./me -2 file.c       # Use a two-page display
./me -l startup.l    # Load a Lisp startup file
```

On Linux, MicroEMACS also loads `./.mel` or `~/.mel` automatically when
Lisp support is enabled. Windows uses `me.l`. Lisp configuration can
define keys, set variables, and select syntax definitions.
At runtime, `C-x F` evaluates `.l`, `.lisp`, and `.mel` files as Lisp;
other files are read as profiles. `M-X read-lisp` always evaluates a
Lisp file.

## Building

The default Linux and Windows builds enable color and Lisp support with
`-DCOLOR` and `-DLISP`. The recommended build commands are:

```sh
./build.sh -b        # Build for the host system
./build.sh -b test   # Build the test targets
./build.sh -d        # Build with debug symbols
./build.sh -r        # Build and strip a local release
./build.sh -m        # Build without color or Lisp
./build.sh -p        # Build a stripped static Linux executable
./build.sh -w        # Build me.exe with MinGW in Docker
./build.sh -D        # Regenerate the PDF manual
./build.sh -c        # Remove generated build files
```

The direct make interface is also available:

```sh
make MF=Make.Linux clean all
make min                         # Minimal Linux build
make MF=Make.Mingw CC=x86_64-w64-mingw32-gcc \
     LD=x86_64-w64-mingw32-gcc clean all
```

Individual features can be selected when invoking the platform makefile:

```sh
make -f Make.Linux COLOR=no LISP=no clean me
make -f Make.Mingw COLOR=yes LISP=no clean me.exe
```

The Windows build uses Docker and the `zhoujd/mingw:base` image by
default. Build the required tool images from the repository root with:

```sh
make -C docker/dockerfiles/tool mingw-base
make -C docker/dockerfiles/tool wine-base
make -C docker/dockerfiles/tool docs-base
```

## Testing

Build and run the source-level tests with:

```sh
./build.sh -b test clean all
./test/syntax-test
./test/display-test
./test/lisp-test
```

Run `./test/cscope-test` when validating cscope changes. For Windows,
build `me.exe` and run the Wine smoke test from the repository root:

```sh
./build.sh -w
./docker/run.sh wine
```

## Documentation

* [`doc/microemacs.md`](doc/microemacs.md) — complete user and build manual.
* [`doc/microemacs.pdf`](doc/microemacs.pdf) — generated PDF version of the manual.
* [`www/building.md`](www/building.md) — web version of the build guide.
* [`doc/guide/content`](doc/guide/content) — source for the Hugo user guide.
* [`examples/logline.pro`](examples/logline.pro) — runnable profile and keyboard macro example.
* [`examples/logline.l`](examples/logline.l) — equivalent Tiny Lisp editing example.
* [`ChangeLog`](ChangeLog) — historical release notes.

Rebuild the PDF with the docs tool image:

```sh
./build.sh -D
```

## License

The original MicroEMACS source was released into the public domain by
Dave Conroy. Enhancements in this tree are licensed under the GNU
General Public License Version 3; see [`LICENSE`](LICENSE).
