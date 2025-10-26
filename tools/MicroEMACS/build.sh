#!/bin/bash

build() {
    make
    echo "Build done"
}

publish() {
    docker run -u $(id -u):$(id -g)  -v ./:/app zhoujd/alpine sh -c '
cd /app
make clean
make LDFLAGS=-static DEBUG=no
make strip
ls -lh em
'
    echo "Publish done"
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

remove() {
    if [ "$(whoami)" == "root" ]; then
        make uninstall
    else
        sudo make uninstall
    fi
    echo "Remove done"
}

usage() {
    app=$(basename $0)
    cat <<EOF
Usage: $app {build|-b|publish|-p|clean|-c|install|-i|remove|-r}
EOF
}

case $1 in
    build|-b )
        build
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
    remove|-r )
        remove
        ;;
    * )
        usage
        ;;
esac
