# Building a MicroEMACS

MicroEMACS is built from portable editor sources plus a small set of
operating-system and terminal sources. The top-level `Makefile` chooses
`Make.Linux` on Linux and `Make.Mingw` on Windows. You can select a
makefile explicitly with `make MF=Make.Linux` or `make MF=Make.Mingw`.

## Operating System

MicroEMACS currently supports Linux and Windows. Support code for other
operating systems has been lost
(in the distant past, these included CP/M-86 and MS-DOS on the DEC Rainbow,
VMS on the VAX, CP/M-68K, GEMDOS, and FlexOS V60/68K/286).
The following modules contain code dependencies on the operating system:

* `sys/unix/ttyio.c` - low level UNIX terminal I/O.
* `sys/mingw/ttyio.c` - low level Windows console I/O.

* `sys/unix/spawn.c` and `sys/mingw/spawn.c` - subjob creation.

* `sys/unix/fileio.c` and `sys/mingw/fileio.c` - low level file handling.


Adding a new operating system consists mostly of adding a corresponding
directory under `sys` and updating one of the makefiles.

## Terminal Support

MicroEMACS supports several kinds of terminals: those
supporting ncurses or termcap, and the native Windows text console
(the code for real-mode PC displays and OS/2 terminal windows has been lost).
The following modules contain code dependencies on the terminal type:

* `tty.c` - high-level terminal support.

* `ttyio.c` - low-level terminal support.

* `ttykbd.c` - keyboard dependencies and extensions.

Changing terminal type consists mostly of changing these files, and the header file `ttydef.h`
These files are located in the `tty/ncurses`, `tty/termcap`, and
`tty/mingw` directories.

Some terminals have memory mapped displays, or interfaces that
act as such.  These include ncurses and Windows text consoles.
Support for these
displays is enabled by setting the MEMMAP switch in `ttydef.h` to 1.
This eliminates the fancy Gosling screen update code in `display.c`,
and enables writing directly to screen memory (or to a screen buffer
that the terminal interface library later writes to the screen).

To support a new memory-mapped display, you must provide a `putline` function
for writing lines to the display.  On old DOS-base systems, this code
was written in assembly language, but on modern terminals it is
written in C and placed in `tty.c`.

## Building with build.sh

The `build.sh` wrapper is the recommended way to build, test, and install
the editor. Run it from `tools/MicroEMACS`:

    ./build.sh -b          # Build the editor for the host system
    ./build.sh -b test     # Build the test targets
    ./build.sh -d          # Build a debug executable
    ./build.sh -r          # Build and strip a local release
    ./build.sh -p          # Build a static executable in Docker
    ./build.sh -w          # Clean and build me.exe with MinGW in Docker
    ./build.sh -i          # Install the Linux executable in /usr/local/bin
    ./build.sh -c          # Remove generated build files

The Linux build enables color support with `-DCOLOR`. To use termcap
instead of ncursesw, pass `TERMCAP=yes` to make:

    make MF=Make.Linux TERMCAP=yes clean all

The release wrapper selects a static build automatically on Alpine and
Void Linux. The publish wrapper builds a stripped static executable in
the `zhoujd/alpine` Docker image.

## Windows Build

`./build.sh -w` builds a stripped `me.exe` with MinGW-w64. Outside
Docker, the wrapper requires Docker and an image whose name defaults to
`zhoujd/mingw:base`. Build that image from the repository root with:

    make -C docker/dockerfiles/tool mingw-base

Override the image and compiler with `MINGW_IMG` and `MINGW_CC` if needed:

    MINGW_IMG=example/mingw:base MINGW_CC=x86_64-w64-mingw32-gcc \
        ./build.sh -w

The Windows build also enables color support with `-DCOLOR`.

## Direct Make Builds

The underlying makefiles remain available:

    make MF=Make.Linux clean all
    make MF=Make.Linux DEBUG=yes clean all
    make MF=Make.Mingw CC=x86_64-w64-mingw32-gcc \
        LD=x86_64-w64-mingw32-gcc clean all

Tests live in `test` and can be built directly:

    make -C test
    make -C test debug

Ruby extensions and PCRE2 are not enabled by the current makefiles.
