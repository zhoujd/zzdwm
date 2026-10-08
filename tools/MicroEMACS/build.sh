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
build|-b        build {test|-t|all|-a|windows|-w}
windows|-w      build Windows me.exe
clean|-c        clean {test|-t|all|-a|windows|-w}
debug|-d        debug
release|-r      release
publish|-p      publish
install|-i      install
uninstall|-u    uninstall
EOF
}

build() {
    case ${1:-""} in
        windows|-w )
            shift
            windows "$@"
            return
            ;;
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

mingw_image() {
    echo "${MINGW_IMG:-${IMG_NS:-zhoujd}/mingw:base}"
}

windows() {
    compiler=${MINGW_CC:-x86_64-w64-mingw32-gcc}
    img=$(mingw_image)

    if [ -n "$INSIDE_DOCKER" ]; then
        if ! command -v "$compiler" >/dev/null 2>&1; then
            echo "MinGW compiler not found: $compiler" >&2
            exit 1
        fi
        make -f Make.Mingw clean || exit 1
        make -f Make.Mingw CC="$compiler" LD="$compiler" "$@" || exit 1
        echo "Build Windows done"
        return
    fi

    if ! command -v docker >/dev/null 2>&1; then
        echo "Docker is required to build me.exe" >&2
        exit 1
    fi
    if ! docker image inspect "$img" >/dev/null 2>&1; then
        echo "MinGW Docker image not found: $img" >&2
        echo "Build it with: make -C docker/dockerfiles/tool mingw-base" >&2
        exit 1
    fi

    if ! docker run \
        --name="build-me-mingw-1" \
        --rm \
        -i \
        -u "$(id -u):$(id -g)" \
        -e INSIDE_DOCKER=1 \
        -e MINGW_CC="$compiler" \
        -v "$MNT_DIR:$MNT_DIR" \
        -w "$WS" \
        "$img" \
        sh -c '
        make -f Make.Mingw clean &&
        make -f Make.Mingw CC="$MINGW_CC" LD="$MINGW_CC" "$@"
        ' sh "$@"
    then
        exit 1
    fi
    echo "Build Windows done"
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
        windows|-w )
            make -f Make.Mingw clean
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
    windows|-w )
        shift
        windows "$@"
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
