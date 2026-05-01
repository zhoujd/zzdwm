#!/bin/sh

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
MNT_DIR=$(git rev-parse --show-toplevel)
WS=$SCRIPT_DIR
TM=Make.Test

[ -f /etc/os-release ] && . /etc/os-release

usage() {
    app=$(basename $0)
    cat <<EOF
Usage: $app {option}
Option:
build|-b        build {test|-t|all|-a}
clean|-c        clean {test|-t|all|-a}
debug|-d        debug
release|-r      release
publish|-p      publish
install|-i      install
uninstall|-u    uninstall
EOF
}

build() {
    case ${1:-""} in
        test|-t )
            shift
            make -f $TM $@
            ;;
        all|-a )
            make
            make -f $TM
            ;;
        -* )
            usage
            exit 0
            ;;
        * )
            make $@
            ;;
    esac
    echo "Build done"
}

debug() {
    make clean
    make debug
    echo "Build debug done"
}

release() {
    make clean
    case $ID in
        alpine|void )
            make LDFLAGS="-static -s -Os"
            ;;
        * )
            make
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
make clean
make LDFLAGS="-static -s -Os"
EOF
    fi
    echo "Build publish done"
}

clean() {
    case ${1:-""} in
        test|-t )
            make -f $TM clean
            ;;
        all|-a )
            make clean
            make -f $TM clean
            ;;
        -* )
            usage
            exit 0
            ;;
        * )
            make clean
            ;;
    esac
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
        shift
        clean "$@"
        ;;
    install|-i )
        install
        ;;
    uninstall|-u )
        uninstall
        ;;
    * )
        usage
        exit 0
        ;;
esac
