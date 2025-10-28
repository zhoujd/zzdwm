#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
CORE_ROOT=$(cd $SCRIPT_ROOT/.. && pwd)
CORE_TOP=$(cd $CORE_ROOT/.. && pwd)

PROJ_ROOT=$CORE_ROOT/tools/MicroEMACS

main() {
    pushd $PROJ_ROOT >/dev/null
    ./build.sh "$@"
    popd >/dev/null
}

main "$@"
