#!/bin/sh

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
MNT_DIR=$(git rev-parse --show-toplevel)
WS=$SCRIPT_DIR

. /etc/os-release

build() {
    ./configure
    make clean
    make CFLAGS="-O0 -g"
    echo "Build done"
}

release() {
    ./configure
    cp -v config.h.static config.h
    make clean
    case $ID in
        alpine|void )
            make CFLAGS="-Os" LDFLAGS="-s -static"
            ;;
        * )
            make CFLAGS="-Os"
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
        opt="
            -i
            -u $(id -u):$(id -g)
            -v $MNT_DIR:$MNT_DIR
            -w $WS
            "
        docker run $opt $img sh <<'EOF'
cat /etc/os-release
./configure
cp -v config.h.static config.h
make clean
make CFLAGS="-Os" LDFLAGS="-s -static"
ls -l screen
EOF
    fi
    echo "Build publish done"
}

clean() {
    git clean -dfx
    echo "Clean done"
}

install() {
    if [ "$(whoami)" = "root" ]; then
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
