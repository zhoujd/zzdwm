#!/bin/bash

declare VERSION=${1:-"8.1.1045"}

docker run -i --rm -v "$PWD":/out -w /root docker.io/alpine:edge /bin/sh <<EOF
apk --no-cache add gcc make musl-dev ncurses-static

wget "https://github.com/vim/vim/archive/v${VERSION}.tar.gz"
tar xvfz "v${VERSION}.tar.gz"

cd "vim-${VERSION}"

LDFLAGS="-static" ./configure \
    --disable-channel \
    --disable-gpm \
    --disable-gtktest \
    --disable-gui \
    --disable-netbeans \
    --disable-nls \
    --disable-selinux \
    --disable-smack \
    --disable-sysmouse \
    --disable-xsmp \
    --enable-multibyte \
    --with-features=huge \
    --without-x \
    --with-tlib=ncursesw

make -j
make install

mkdir -p /out/vim
cp -r /usr/local/* /out/vim

strip /out/bin/vim
chown -R $(id -u):$(id -g) /out/vim
EOF
