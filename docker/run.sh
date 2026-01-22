#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
TOP=$(cd $SCRIPT_ROOT/.. && pwd)
MNT=$HOME
CTN_NAME=zz-build-1
CTN_HOST=$CTN_NAME
CTN_USER=$(id -u):$(id -g) ## root

IMGS=(
    zhoujd/alpine:base
    zhoujd/void-linux:base
    zhoujd/ubuntu:base
)

RUN_PARAM=(
    -it
    --rm
    --name=$CTN_NAME
    --user=$CTN_USER
    --group-add root
    --privileged=true
    --cap-add=ALL
    -h $CTN_HOST
    -e HOME=$MNT
    -e INSIDE_DOCKER=yes
    -v $TOP:$MNT/$(basename $TOP)
    -w $MNT/$(basename $TOP)
)

run() {
    local kind=$1
    case $kind in
        alpine|-a )
            shift
            img=${IMGS[0]}
            ;;
        void|-v )
            shift
            img=${IMGS[1]}
            ;;
        ubuntu|-u )
            shift
            img=${IMGS[2]}
            ;;
        * )
            img=${IMGS[0]}
            ;;
    esac
    local ext=()
    if [ "$1" == "--ext" ]; then
        shift
        ext+=($@)
    fi
    docker stop $CTN >/dev/null 2>&1
    docker rm $CTN >/dev/null 2>&1
    docker run ${RUN_PARAM[@]} ${ext[@]} $img
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
usage: $app {build|-b|run|-r|status|-s|clean|-c|purge}
run|-r   --   {alpine|-a|void|-v|ubuntu|-u}
EOF
}

case $1 in
    build|-b )
        make -C dockerfiles
        ;;
    run|-r )
        shift
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
