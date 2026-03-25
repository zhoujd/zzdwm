#!/bin/sh

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
MNT_DIR=$(git rev-parse --show-toplevel)
WS=$SCRIPT_DIR
TM=test.mak

. /etc/os-release

build() {
    case ${1:-""} in
        test|-t )
            shift
            make -f $TM $@
            ;;
        * )
            make $@
            ;;
    esac
    echo "Build done"
}

debug() {
    make clean
    make DEBUG=yes
    echo "Build debug done"
}

release() {
    make clean
    case $ID in
        alpine|void )
            make STATIC=yes
            ;;
        * )
            make
            ;;
    esac
    make strip
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
make STATIC=yes
make strip
EOF
    fi
    echo "Build publish done"
}

clean() {
    case ${1:-""} in
        test|-t )
            make -f $TM clean
            ;;
        * )
            make clean
            ;;
    esac
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
usage: $app {option}
option:
build|-b            build {test|-t}
clean|-c            clean {test|-t}
debug|-d            debug
release|-r          release
publish|-p          publish
install|-i          install
uninstall|-u        uninstall
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
        ;;
esac
