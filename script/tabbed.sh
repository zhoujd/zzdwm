#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
CORE_ROOT=$(cd $SCRIPT_ROOT/.. && pwd)
CORE_TOP=$(cd $CORE_ROOT/.. && pwd)

PROJ_ROOT=$CORE_ROOT/tools/tabbed

dep() {
    sudo apt install -y libx11-dev libxft-dev libfontconfig-dev
    echo "Install dep done"
}

build() {
    pushd $PROJ_ROOT
    make
    sudo make install
    popd
    echo "Build done"
}

clean() {
    pushd $PROJ_ROOT
    git clean -dfx
    popd
    echo "Clean done"
}

usage() {
    app=$(basename $0)
    cat <<EOF
Usage: $app {dep|build|-b|clean|-c}
EOF
}

case $1 in
    dep )
        dep
        ;;
    build|-b )
        build
        ;;
    clean|-c )
        clean
        ;;
    * )
        usage
        ;;
esac
