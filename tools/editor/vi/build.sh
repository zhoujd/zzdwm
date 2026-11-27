#!/bin/sh

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
MNT_DIR=$(git rev-parse --show-toplevel)
WS=$SCRIPT_DIR
TEST_DIR=test

[ -f /etc/os-release ] && . /etc/os-release

usage() {
    app=$(basename $0)
    cat <<EOF
Usage: $app {option}
Option:
build|-b        build {test|-t}
publish|-p      publish Linux static vi [--upx|-u]
debug|-d        debug
release|-r      release
clean|-c        clean
install|-i      install
uninstall|-u    uninstall
EOF
}

build() {
    case ${1:-""} in
        test|-t )
            shift
            python3 "$TEST_DIR/vi_e2e.py" "$@"
            ;;
        -* )
            usage
            exit 1
            ;;
        * )
            make $@
            ;;
    esac
    echo "Build done"
}

release() {
    make clean
    case $ID in
        alpine|void )
            make STATIC=yes strip
            ;;
        * )
            make strip
            ;;
    esac
    echo "Build release on $ID done"
}

publish() {
    CMD=${1:-}
    if [ -n "$INSIDE_DOCKER" ]; then
        echo "Build release"
        release
    else
        img=zhoujd/alpine
        docker run \
            --name="build-vi-1" \
            --rm \
            -i \
            -u "$(id -u):$(id -g)" \
            -e INSIDE_DOCKER=1 \
            -v "$MNT_DIR:$MNT_DIR" \
            -w "$WS" \
            "$img" \
            sh -c "
            cat /etc/os-release &&
            make clean &&
            make STATIC=yes strip
            " || exit 1
    fi
    case $CMD in
        --upx|-u )
            upx --ultra-brute vi
            ;;
    esac
    echo "Build publish done"
}

debug() {
    make clean
    make DEBUG=yes
    echo "Build debug done"
}

clean() {
    make clean
    echo "Clean done"
}

install() {
    if [ "$(id -u)" -eq 0 ]; then
        make install
    else
        sudo make install
    fi
    echo "Install done"
}

uninstall() {
    if [ "$(id -u)" -eq 0 ]; then
        make uninstall
    else
        sudo make uninstall
    fi
    echo "Uninstall done"
}

case $1 in
    build|-b )
        shift
        build "$@"
        ;;
    publish|-p )
        shift
        publish "$@"
        ;;
    debug|-d )
        debug
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
