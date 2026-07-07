#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
TOP=$(cd $SCRIPT_ROOT/.. && pwd)

source ${SCRIPT_ROOT}/env.sh

CTN_USER=zach
CTN_HOST=build
CTN_PREFIX=zz-build
CTN_NAME=${CTN_PREFIX}-1

MNT=/home/$CTN_USER
WS=$MNT/$(basename $TOP)

IMGS=(
    zhoujd/alpine:base
    zhoujd/void-linux:base
    zhoujd/ubuntu:base
)

RUN_PARAM=(
    --rm
    --name=$CTN_NAME
    --user=$CTN_USER
    --privileged=true
    --cap-add=ALL
    --network=host
    --add-host="$CTN_HOST:127.0.1.1"
    -h $CTN_HOST
    -e INSIDE_DOCKER=yes
    -v $TOP:$WS
    -w $WS
)

stop() {
    docker kill ${CTN_NAME} >/dev/null 2>&1 \
        && echo "Stop ${CTN_NAME} done" \
        && sleep 1s
}

run() {
    local kind=$1
    local cmd="bash -l"
    local opt=(
        -it
    )
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
    if [ "$1" == "+++" ]; then
        shift
        opt+=($@)
    fi
    stop
    docker run ${RUN_PARAM[@]} ${opt[@]} ${img} ${cmd}
}

ssh() {
    local kind=$1
    local cmd="run"
    local port=2222
    local opt=(
        -d
        -e PORT=${port}
        -v ~/.ssh:/home/$CTN_USER/.ssh:ro
    )
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
    if [ "$1" == "+++" ]; then
        shift
        opt+=($@)
    fi
    stop
    docker run ${RUN_PARAM[@]} ${opt[@]} ${img} ${cmd} >/dev/null 2>&1 \
        && echo "Start SSH server on ${port}"
}

shell() {
    local cmd="bash -l"
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
    echo "Clean rm exit"
    cleanexit
    echo "Clean none images"
    cleannone
}

valgrind() {
    echo "Run valgrind check"
    local prog=${1:-""}
    local img=zhoujd/valgrind:latest
    local cmd="valgrind --leak-check=full $prog"
    docker run \
             --rm \
             --cap-add=SYS_PTRACE \
             -v $(pwd):/workspace \
             $img \
             $cmd
}

usage() {
    local app=$(basename $0)
    cat <<EOF
Usage: $app {option} {args}
Option:
run|-r        Run container {args}
ssh           Start ssh {args}
build|-b      Build images
clean|-c      Clean
shell|-s      Attach shell
stop          Stop service
status        Show status
valgrind|-v   Run valgrind check
Args:
alpine|-a     Alpine (default)
void|-v       Void Linux
ubuntu|-u     Ubuntu
+++           Addition parameters
EOF
}

CMD=${1:-""}
case $CMD in
    build|-b )
        make -C dockerfiles
        ;;
    run|-r )
        shift
        run "$@"
        ;;
    ssh )
        shift
        ssh "$@"
        ;;
    shell|-s )
        shift
        shell "$@"
        ;;
    stop )
        stop
        ;;
    status )
        status
        ;;
    clean|-c )
        clean
        ;;
    valgrind|-v )
        shift
        valgrind "$@"
        ;;
    * )
        usage
        ;;
esac
