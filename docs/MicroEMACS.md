MicroEMACS
==========

## URLs

    ## https://github.com/troglobit/MicroEMACS

## Linux editors

    ## Portable Mg
    ## https://github.com/hboetes/mg
    ## https://github.com/hboetes/mg/releases
    $ wget https://github.com/hboetes/mg/releases/download/20240709/mg-20240709-static-amd64
    $ sudo cp mg-20240709-static-amd64 /usr/local/bin/mg

    ## Micro (GNU) Emacs
    ## https://github.com/troglobit/mg
    ## https://taingram.org/blog/lightweight-emacs-editors.html

    ## Textadept
    ## https://orbitalquark.github.io/textadept/

    ## The original MicroEMACS
    ## https://github.com/troglobit/MicroEMACS

    ## Jasspa MicroEmacs (standalone executables)
    ## https://github.com/mittelmark/microemacs
    ## https://github.com/mittelmark/microemacs/releases/tag/v09.12.21

    ## Jasspa MicroEmacs (More with GDB)
    ## https://github.com/bjasspa/jasspa
    ## https://github.com/bjasspa/jasspa/releases

## Start MicroEMACS

    #!/bin/bash
    mkdir -p ~/.me
    touch ~/.me/me.emf
    export MEUSERPATH=~/.me
    mec "$@"

## Create statically-linked binary that uses getaddrinfo

    ## glibc uses libnss to support a number of different providers for address resolution services
    ## Meanwhile in version 2.20 there is the --enable-static-nss flag of configure which seems to do exactly this

    ## You can use musl library to replace glibc.
    ## To use musl, you can either install it and build your software using musl-gcc,
    ## or you can use a Linux distribution that uses musl, e.g. Alpine Linux.
