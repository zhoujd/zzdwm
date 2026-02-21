#!/bin/sh

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
MNT_DIR=$(cd $SCRIPT_DIR/../.. && pwd)
WS=$MNT_DIR/tools/$(basename $SCRIPT_DIR)

build() {
    make $@
    echo "Build done"
}

debug() {
    make clean
    make DEBUG=yes
    echo "Build debug done"
}

release() {
    make clean
    make STATIC=yes
    make strip
    echo "Build release done"
}

publish() {
    if [ -n "$INSIDE_DOCKER" ]; then
        echo "Build release"
        release
    else
        img=zhoujd/alpine
        opt="
            -i \
            -u $(id -u):$(id -g) \
            -v $MNT_DIR:$MNT_DIR \
            -w $WS \
        "
        docker run $opt $img sh <<'EOF'
cat /etc/os-release
make clean
make STATIC=yes
make strip
EOF
    fi
    echo "Build publish done"
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
