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
clean|-c        clean {test|-t|all|-a|windows|-w}
debug|-d        debug
release|-r      release
publish|-p      publish
install|-i      install
uninstall|-u    uninstall
dep|-D          install the MinGW cross compiler
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

windows() {
    compiler=${MINGW_CC:-x86_64-w64-mingw32-gcc}
    if ! command -v "$compiler" >/dev/null 2>&1; then
        echo "MinGW compiler not found: $compiler" >&2
        echo "Set MINGW_CC to another MinGW cross compiler." >&2
        exit 1
    fi

    make -f Make.Mingw clean
    make -f Make.Mingw CC="$compiler" LD="$compiler" "$@"
    echo "Build Windows done"
}

dep() {
    compiler=${MINGW_CC:-x86_64-w64-mingw32-gcc}
    compiler_name=$(basename "$compiler")

    case "$compiler_name" in
        x86_64-w64-mingw32-gcc )
            target=x86_64
            debian_target=x86-64
            ;;
        i686-w64-mingw32-gcc )
            target=i686
            debian_target=i686
            ;;
        * )
            echo "Unsupported MinGW compiler: $compiler" >&2
            return 1
            ;;
    esac

    if command -v "$compiler" >/dev/null 2>&1; then
        echo "Cross compiler already installed: $compiler"
        return 0
    fi

    if command -v apt-get >/dev/null 2>&1; then
        package=gcc-mingw-w64-$debian_target-posix
        if [ "$(id -u)" -eq 0 ]; then
            apt-get update
            apt-get install -y "$package"
        else
            sudo apt-get update
            sudo apt-get install -y "$package"
        fi
    elif command -v dnf >/dev/null 2>&1; then
        case "$target" in
            x86_64 )
                package=mingw64-gcc
                ;;
            i686 )
                package=mingw32-gcc
                ;;
        esac
        if [ "$(id -u)" -eq 0 ]; then
            dnf install -y "$package"
        else
            sudo dnf install -y "$package"
        fi
    elif command -v pacman >/dev/null 2>&1; then
        package=mingw-w64-gcc
        if [ "$(id -u)" -eq 0 ]; then
            pacman -Sy --needed "$package"
        else
            sudo pacman -Sy --needed "$package"
        fi
    elif command -v apk >/dev/null 2>&1; then
        package=mingw-w64-gcc
        if [ "$(id -u)" -eq 0 ]; then
            apk add "$package"
        else
            sudo apk add "$package"
        fi
    elif command -v xbps-install >/dev/null 2>&1; then
        package=cross-$target-w64-mingw32
        if [ "$(id -u)" -eq 0 ]; then
            xbps-install -Sy "$package"
        else
            sudo xbps-install -Sy "$package"
        fi
    else
        echo "Unsupported package manager for MinGW compiler installation" >&2
        return 1
    fi

    if ! command -v "$compiler" >/dev/null 2>&1; then
        echo "MinGW compiler installation failed: $compiler" >&2
        return 1
    fi

    echo "Cross compiler installed: $compiler"
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
    dep|-D )
        dep
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
