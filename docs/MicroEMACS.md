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

## Start ME

    #!/bin/bash
    mkdir -p ~/.me
    touch ~/.me/me.emf
    export MEUSERPATH=~/.me
    mec "$@"
