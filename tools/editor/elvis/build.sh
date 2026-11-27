#!/bin/sh

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
MNT_DIR=$(git rev-parse --show-toplevel)
WS=$SCRIPT_DIR

[ -f /etc/os-release ] && . /etc/os-release

build() {
    ./configure --without-x linux
    make clean
    make CC="gcc -O0 -g"
    echo "Build done"
}

release() {
    ./configure --without-x linux
    make clean
    case $ID in
        alpine|void )
            make CC="gcc -Os -s -static"
            ;;
        * )
            make CC="gcc -Os -s"
            ;;
    esac
    echo "Build release on $ID done"
}

publish() {
    if [ -n "$INSIDE_DOCKER" ]; then
        echo "Build release"
        release
    else
        img=zhoujd/alpine
        docker run \
            --name="build-elvis-1" \
            --rm \
            -i \
            -u "$(id -u):$(id -g)" \
            -e INSIDE_DOCKER=1 \
            -v "$MNT_DIR:$MNT_DIR" \
            -w "$WS" \
            "$img" \
            sh -c '
            cat /etc/os-release &&
            ./configure --without-x linux &&
            make clean &&
            make CC="gcc -Os -s -static"
            ' || exit 1
        upx --ultra-brute elvis fmt ctags
    fi
    echo "Build publish done"
}

clean() {
    git clean -dfx
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

usage() {
    app=$(basename $0)
    cat <<EOF
usage: $app {build|-b|release|-r|publish|-p|clean|-c|install|-i|uninstall|-u}
EOF
}

case $1 in
    build|-b )
        build
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
