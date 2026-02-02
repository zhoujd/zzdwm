#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
TOP=$(cd $SCRIPT_ROOT/.. && pwd)

CTN_NAME=zz-build-1
CTN_USER=zach
MNT=/home/$CTN_USER
WS=$MNT/$(basename $TOP)

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
    --privileged=true
    --cap-add=ALL
    --network=host
    -e INSIDE_DOCKER=yes
    -v $TOP:$WS
    -w $WS
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
    local add=()
    if [ "$1" == "+++" ]; then
        shift
        add+=($@)
    fi
    docker stop $CTN_NAME >/dev/null 2>&1
    docker rm $CTN_NAME >/dev/null 2>&1
    docker run ${RUN_PARAM[@]} ${add[@]} ${img}
}

shell() {
    local cmd="bash"
    local opt=(
        -it
    )
    if [ -n "$(docker ps -f "name=${CTN_NAME}" -f "status=running" -q)" ]; then
        echo "Attach shell to ${CTN_NAME}"
        docker exec ${opt[@]} ${CTN_NAME} ${cmd}
    else
        echo "No ${CTN_NAME} running"
    fi
}

status() {
    docker ps -a | grep ${CTN_NAME}
}

clean() {
    echo "Clean $CTN_NAME"
    docker stop $CTN_NAME >/dev/null 2>&1
    docker rm $CTN_NAME >/dev/null 2>&1
}

purge() {
    echo "Clean none images"
    img_list=$(docker images --filter "dangling=true" -q --no-trunc)
    if [ -n "$img_list" ]; then
        docker rmi $img_list
    fi
}

usage() {
    local app=$(basename $0)
    cat <<EOF
usage: $app {build|-b|run|-r|shell|-s|clean|-c|status|purge}
run|-r   --   {alpine|-a|void|-v|ubuntu|-u|+++}
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
    shell|-s )
        shift
        shell "$@"
        ;;
    clean|-c )
        clean
        ;;
    status )
        status
        ;;
    purge )
        purge
        ;;
    * )
        usage
        ;;
esac
