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

remove() {
    if [ "$(whoami)" == "root" ]; then
        make uninstall
    else
        sudo make uninstall
    fi
    echo "Remove done"
}

usage() {
    app=$(basename $0)
    cat <<EOF
Usage: $app {build|-b|clean|-c|install|-i|remove|-r}
EOF
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
    remove|-r )
        remove
        ;;
    * )
        usage
        ;;
esac
