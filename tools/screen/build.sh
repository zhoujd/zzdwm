#!/bin/sh

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
MNT_DIR=$(git rev-parse --show-toplevel)
WS=$SCRIPT_DIR

[ -f /etc/os-release ] && . /etc/os-release

build() {
    ./configure \
        --prefix=/usr/local \
        --enable-colors256 \
        --disable-pam \
        --disable-telnet
    cp -vf config.h.static config.h
    make clean
    make CFLAGS="-O0 -g"
    echo "Build done"
}

release() {
    ./configure \
        --prefix=/usr/local \
        --enable-colors256 \
        --disable-pam \
        --disable-telnet
    cp -vf config.h.static config.h
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
        img=zhoujd/alpine:latest
        opt="
            --name=build-screen-1
            --rm
            -i
            -u $(id -u):$(id -g)
            -v $MNT_DIR:$MNT_DIR
            -w $WS
            "
        docker run $opt $img sh <<'EOF'
cat /etc/os-release
./configure \
    --prefix=/usr/local \
    --enable-colors256 \
    --disable-pam \
    --disable-telnet
cp -vf config.h.static config.h
make clean
make CFLAGS="-Os" LDFLAGS="-s -static"
strip -v screen
ls -l screen
EOF
    upx --ultra-brute screen
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
