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
            exit 1
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
    CMD=${1:-}
    if [ -n "$INSIDE_DOCKER" ]; then
        echo "Build release"
        release
    else
        img=zhoujd/alpine
        HOST_UID=$(id -u)
        HOST_GID=$(id -g)
        docker run \
            --name="build-me-1" \
            --rm \
            -i \
            -u root \
            -e INSIDE_DOCKER=1 \
            -v "$MNT_DIR:$MNT_DIR" \
            -w "$WS" \
            "$img" \
            sh -c "
            cat /etc/os-release
            make clean
            make STATIC=yes
            make strip
            chown -R $HOST_UID:$HOST_GID .
            "
        case $CMD in
            --upx|-u )
                upx --ultra-brute me
                ;;
        esac
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
            ;;
        * )
            make clean
            ;;
    esac
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
    debug|-d )
        debug
        ;;
    release|-r )
        release
        ;;
    publish|-p )
        shift
        publish "$@"
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
