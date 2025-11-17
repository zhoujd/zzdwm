#!/bin/bash

build() {
    make
    echo "Build done"
}

release() {
    make clean
    make LDFLAGS=-static DEBUG=no
    make strip
    ls -lh em
    echo "Release done"
}

publish() {
    local img=zhoujd/alpine
    local opt=(
        -i # keeps standard input open
        -u $(id -u):$(id -g)
        -v $(pwd):$(pwd)
        -w $(pwd)
    )
    docker run ${opt[@]} $img sh <<'EOF'
make clean
make LDFLAGS=-static DEBUG=no
make strip
EOF
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
