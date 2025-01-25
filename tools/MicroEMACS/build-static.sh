#!/bin/sh

build() {
    make LDFLAGS=-static
    echo "Build done"
}

clean() {
    make clean
    echo "Clean done"
}

install() {
    if [ "$(whoami)" == "root" ]; then
        make install
    else
        sudo make install
    fi
    echo "Install done"
}

case $1 in
    build|-b )
        build
        ;;
    clean|-c )
        clean
        ;;
    install|-i )
        install
        ;;
    * )
        echo "Usage: $(basename $0) {build|-b|clean|-c|install|-i}"
        ;;
esac
