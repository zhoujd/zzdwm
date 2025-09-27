#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
TOP=$(cd $SCRIPT_ROOT/.. && pwd)
CTN=zz-build-1
MNT=/root
IMGS=(
    zhoujd/alpine:base
    zhoujd/void-linux:base
)

RUN_PARAM=(
    -it
    --name $CTN
    --privileged=true
    --cap-add=ALL
    -v $TOP:$MNT/$(basename $TOP)
)

run() {
    local kind=$1
    case $kind in
        alpine|-a )
            img=${IMGS[0]}
            ;;
        void|-v )
            img=${IMGS[1]}
            ;;
        * )
            img=${IMGS[0]}
            ;;
    esac
    docker run ${RUN_PARAM[@]} $img
    echo "Run Done"
}

status() {
    echo "[IMG]"
    for img in ${IMGS[@]}; do
        docker images | grep $img
    done
    echo "[PS]"
    docker ps -a | grep $CTN
    echo "Status Done"
}

clean() {
    docker stop $CTN >/dev/null 2>&1
    docker rm $CTN >/dev/null 2>&1
    echo "Clean Done"
}

usage() {
    local app=$(basename $0)
    cat <<EOF
Usage: $app {build|-b|run|-r|status|-s|clean|-c|purge}
run|-r   --   {alpine|-a|void|-v}
EOF
}

case $1 in
    build|-b )
        make -C dockerfiles
        ;;
    run|-r )
        shift
        clean
        run "$@"
        ;;
    status|-s )
        status
        ;;
    clean|-c )
        clean
        ;;
    purge )
        docker system prune -f -a
        ;;
    * )
        usage
        ;;
esac
