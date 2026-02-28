#!/bin/sh

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

build() {
    make
    echo "Build done"
}

release() {
    make clean
    make LDFLAGS="-static -s" CFLAGS="-Os -Wno-cpp"
    echo "Release done"
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

uninstall() {
    if [ "$(whoami)" == "root" ]; then
        make uninstall
    else
        sudo make uninstall
    fi
    echo "Uninstall done"
}

usage() {
    app=$(basename $0)
    cat <<EOF
usage: $app {build|-b|release|-r|clean|-c|install|-i|uninstall|-u}
EOF
}

case $1 in
    build|-b )
        build
        ;;
    release|-r )
        release
        ;;
    clean|-c )
        clean
        ;;
    install|-i )
        install
        ;;
    uninstall|-u )
        uninstall
        ;;
    * )
        usage
        ;;
esac
