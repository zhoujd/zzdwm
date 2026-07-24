#!/bin/bash

## Test
# vim -c "set shortmess+=I" -u NONE -U NONE --noplugin

declare VERSION=${1:-"9.2.0843"}

docker run -i --rm -v "$PWD":/out -u root -w /root zhoujd/alpine:latest /bin/sh <<EOF
set -e

apk --no-cache add wget

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

strip /out/vim/bin/vim
chown -R $(id -u):$(id -g) /out/vim
EOF
