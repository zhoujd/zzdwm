MicroEMACS
==========

## URLs

```
## https://github.com/bloovis/microemacs.mirror
## https://github.com/troglobit/MicroEMACS
## https://git.sr.ht/~tekk/microemacs
## https://github.com/DigitalMars/me
```

## Current Source Tree

This repository contains the MicroEMACS editor under `tools/MicroEMACS`.
The current build supports Linux with ncursesw or termcap and Windows with
MinGW-w64. Color support is compiled in with `-DCOLOR`.

```bash
cd tools/MicroEMACS

./build.sh -b        # host build
./build.sh -b test   # tests
./build.sh -p        # static Linux build
./build.sh -w        # Windows me.exe build in Docker
```

The editor supports UTF-8, C/C++, Bash, Python, Markdown, Lisp, and
Emacs Lisp syntax highlighting. Colors and Vim-style line numbers can be
disabled or enabled at runtime:

```bash
me -C file.c         # start with color disabled
me -n file.c         # start with line numbers enabled
```

The unbound extended commands are `set-color` and `set-number`. They can be
entered with `M-X`; a numeric argument of zero disables the feature and a
nonzero argument enables it.

## Linux editors

```
## Bloovis MicroEmacs (pe)
## https://github.com/bloovis/microemacs.mirror
## https://www.bloovis.com/fossil/home/marka/fossils/pe/doc/trunk/www/index.md

## Portable Mg
## https://github.com/hboetes/mg
## https://github.com/hboetes/mg/releases
$ wget https://github.com/hboetes/mg/releases/download/20240709/mg-20240709-static-amd64
$ sudo cp mg-20240709-static-amd64 /usr/local/bin/mg

## Micro (GNU) Emacs
## https://github.com/troglobit/mg
## https://taingram.org/blog/lightweight-emacs-editors.html

## The original MicroEMACS
## https://github.com/troglobit/MicroEMACS

## Jasspa MicroEmacs (standalone executables)
## https://github.com/mittelmark/microemacs
## https://github.com/mittelmark/microemacs/releases/tag/v09.12.21

## Jasspa MicroEmacs (More with GDB)
## https://github.com/bjasspa/jasspa
## https://github.com/bjasspa/jasspa/releases
```

## Start MicroEMACS

```bash
#!/bin/bash
mkdir -p ~/.me
touch ~/.me/me.emf
export MEUSERPATH=~/.me
mec "$@"
```

## Create statically-linked binary that uses getaddrinfo

```bash
## glibc uses libnss to support a number of different providers for address resolution services
## Meanwhile in version 2.20 there is the --enable-static-nss flag of configure which seems to do exactly this

## You can use musl library to replace glibc.
## To use musl, you can either install it and build your software using musl-gcc,
## or you can use a Linux distribution that uses musl, e.g. Alpine Linux.

## STATIC BUILDS on an alpine linux system
## https://github.com/linuxcontainers/
## https://github.com/linuxcontainers/alpine
## https://github.com/linuxcontainers/alpine/pkgs/container/alpine
$ docker pull ghcr.io/linuxcontainers/alpine:latest
$ docker pull docker.io/amd64/alpine:latest
$ cat << EOF > Dockerfile
FROM ghcr.io/linuxcontainers/alpine:3.20
RUN apk update --no-cache \
        && apk upgrade --no-cache \
        && apk add \
        libbsd-static libbsd-dev ncurses-dev musl-dev ncurses-static \
        git gcc make openssh \
        && rm -rf /var/cache/apk/*
CMD ["/bin/sh"]
EOF
```

## Install mg building dependence

```bash
$ sudo apt install libbsd-dev
```

## Modification of the MicroEmacs Executable

```bash
## http://www.jasspa.com/zeroinst.html
## Install bfs tool
$ wget http://www.jasspa.com/development/me-standalone/bfs-v0.1.2.tar.gz

## https://github.com/mittelmark/microemacs/blob/master/README-standalone.md#Modi
## Detach the current archive from the stand-alone executable into a folder mearchive like this
$ bfs -x mearchive mec.bin
or
$ tfs -x mearchive mec.bin

## Add your own or replace changed files
$ bfs -a mec ./mearchive
$ bfs -a mec -o me.bin ./mearchive
or
$ tfs -a mec ./mearchive
$ tfs -a mec -o me.bin ./mearchive
```

## The bloovis (pe) search last

```
M-H  search-again
```

## Piping Input into MicroEMACS

```
$ me <(directory_listing_command)
$ me <(ls -la)
```

## Piping Output Out of MicroEMACS

```
$ me temp_buffer.txt && cat temp_buffer.txt | grep "pattern" && rm temp_buffer.txt
```
