#!/bin/bash

SCRIPT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
TOP=$(cd "$SCRIPT_ROOT/.." && pwd)

[ -f "${SCRIPT_ROOT}/env.sh" ] && source "${SCRIPT_ROOT}/env.sh"

CTN_USER="${CTN_USER:-zach}"
CTN_HOST="${CTN_HOST:-build}"
CTN_PREFIX="${CTN_PREFIX:-zz-build}"
CTN_NAME="${CTN_PREFIX}-1"

MNT_ROOT="/home/$CTN_USER"
MNT_KEYS="/mnt/sshkeys"
WS="$MNT_ROOT/$(basename "$TOP")"

IMGS=(
    "zhoujd/alpine:base"
    "zhoujd/void-linux:base"
    "zhoujd/ubuntu:base"
)

RUN_PARAM=(
    --rm
    --name="$CTN_NAME"
    --user="$CTN_USER"
    --privileged=true
    --cap-add=ALL
    --network=host
    --add-host="$CTN_HOST:127.0.1.1"
    -h "$CTN_HOST"
    -e INSIDE_DOCKER=yes
    -v "$HOME/.ssh:$MNT_KEYS"
    -v "$TOP:$WS"
    -w "$WS"
)

# Resolve image name from distro flag
get_image() {
    case "$1" in
        alpine|-a )  echo "${IMGS[0]}" ;;
        void|-v )    echo "${IMGS[1]}" ;;
        ubuntu|-u )  echo "${IMGS[2]}" ;;
        * )          echo "${IMGS[0]}" ;;
    esac
}

stop() {
    if [ -n "$(docker ps -a -q -f "name=^/${CTN_NAME}$")" ]; then
        docker rm -f "${CTN_NAME}" >/dev/null 2>&1
        echo "Stopped ${CTN_NAME}"
        sleep 1
    fi
}

run() {
    local kind="${1:-alpine}"
    local img=$(get_image "$kind")
    [ $# -gt 0 ] && shift

    local opt=(-it)
    if [ "$1" = "+++" ]; then
        shift
        opt+=("$@")
    fi

    stop
    docker run "${RUN_PARAM[@]}" "${opt[@]}" "$img" bash -l
}

ssh() {
    local kind="${1:-alpine}"
    local img=$(get_image "$kind")
    [ $# -gt 0 ] && shift

    local port=2222
    local opt=(
        -d
        -e PORT="${port}"
    )

    if [ "$1" = "+++" ]; then
        shift
        opt+=("$@")
    fi

    stop
    docker run "${RUN_PARAM[@]}" "${opt[@]}" "$img" run >/dev/null 2>&1 \
        && echo "Started SSH server on port ${port}"
}

shell() {
    if [ -n "$(docker ps -f "name=^/${CTN_NAME}$" -f "status=running" -q)" ]; then
        echo "Attaching shell to ${CTN_NAME}..."
        docker exec -it "${CTN_NAME}" bash -l
    else
        echo "Error: Container '${CTN_NAME}' is not running."
    fi
}

status() {
    docker ps -a --filter "name=^/${CTN_NAME}$"
}

clean() {
    echo "Cleaning exited containers..."
    cleanexit 2>/dev/null || docker container prune -f
    echo "Cleaning dangling images..."
    cleannone 2>/dev/null || docker image prune -f
}

valgrind() {
    echo "Usage inside container: valgrind --leak-check=full ./program"
    docker run --rm -it \
        --privileged=true \
        --cap-add=ALL \
        -h valgrind \
        -v "$TOP:/workspace" \
        -w /workspace \
        zhoujd/valgrind:latest bash
}

wine() {
    echo "Usage inside container: wineconsole --backend=curses me.exe"
    docker run --rm -it \
        --privileged=true \
        --cap-add=ALL \
        -h wine \
        -v "$TOP:/workspace" \
        -w /workspace \
        zhoujd/wine:latest bash
}

usage() {
    local app=$(basename "$0")
    cat <<EOF
Usage: $app {command} [distro] [+++ extra_docker_args]

Commands:
run|-r      Run interactive container
ssh         Start SSH daemon background service
shell|-s    Attach a new shell to running container
stop        Stop running container
status      Show container status
build|-b    Build image stack
clean|-c    Clean stopped containers & untagged images
valgrind|-v Run Valgrind container environment
wine|-w     Run Wine container environment

Distros:
alpine|-a   Alpine Linux (default)
void|-v     Void Linux
ubuntu|-u   Ubuntu Linux
EOF
}

CMD="${1:-""}"
case "$CMD" in
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
    wine|-w )
        shift
        wine "$@"
        ;;
    * )
        usage
        ;;
esac
