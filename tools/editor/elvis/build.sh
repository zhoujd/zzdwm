#!/bin/sh

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

build() {
    $SCRIPT_DIR/configure --without-x
    make clean
    make "$@"
    echo "Build done"
}

debug() {
    $SCRIPT_DIR/configure --without-x
    make clean
    make CC="gcc -O0 -g"
    echo "Build debug done"
}

release() {
    $SCRIPT_DIR/configure --without-x
    make clean
    make CC="gcc -Os -s"
    echo "Build release done"
}

publish() {
    $SCRIPT_DIR/configure --without-x
    make clean
    make CC="gcc -Os -s -static"
    echo "Build publish done"
}

clean() {
    git clean -dfx
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
usage: $app {build|-b|debug|-d|release|-r|publish|-p|clean|-c|install|-i|uninstall|-u}
EOF
}

case $1 in
    build|-b )
        shift
        build "$@"
        ;;
    debug|-d )
        debug
        ;;
    release|-r )
        release
        ;;
    publish|-p )
        publish 
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
