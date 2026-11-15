#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
TOP=$(cd $SCRIPT_ROOT/../.. && pwd)

CTN_NAME=codex
CTN_USER=zach
IMG=zhoujd/codex:latest

MNT=/home/$CTN_USER/work
WS=$MNT/$(basename $PWD)

## The container may be launched from a shell that has not sourced the Codex
## key file; load it explicitly so env_key in config.toml is satisfied.
if [ -z "${OPENAI_API_KEY:-}" ] && [ -f "$HOME/.codex/.env" ]; then
    source "$HOME/.codex/.env"
fi

RUN_PARAM=(
    --rm
    --name=$CTN_NAME
    --user=$CTN_USER
    --network=host
    -e TERM=${TERM:-xterm-256color}
    -e OPENAI_API_KEY=${OPENAI_API_KEY-}
    -v $HOME/.codex:/home/$CTN_USER/.codex
    -v $PWD:$WS
    -w $WS
)

check() {
    if ! docker image inspect $IMG >/dev/null 2>&1; then
        echo "Missing $IMG; build it first:" >&2
        echo "    make -C $TOP/docker/dockerfiles/tool codex-base" >&2
        exit 1
    fi
    if [ "$(id -u)" != 1000 ]; then
        echo "Warning: host uid $(id -u) != 1000, mounted files will be owned by 1000" >&2
    fi
}

run() {
    check
    docker run -it ${RUN_PARAM[@]} $IMG codex "$@"
}

exec_cmd() {
    check
    docker run ${RUN_PARAM[@]} $IMG codex exec "$@"
}

shell() {
    if [ -n "$(docker ps -f "name=${CTN_NAME}" -f "status=running" -q)" ]; then
        echo "Attach shell to ${CTN_NAME}"
        docker exec -it ${CTN_NAME} bash -l
    else
        check
        docker run -it ${RUN_PARAM[@]} $IMG bash -l
    fi
}

status() {
    docker ps -a | grep ${CTN_NAME}
}

pull() {
    echo "Pull $IMG"
    docker pull $IMG
    echo "Pull done"
}

usage() {
    local app=$(basename $0)
    cat <<EOF
Usage: $app {option} [args]
Option:
run|-r        interactive codex in a container [codex args]
exec|-e       non-interactive execution, e.g. $app -e "say ok"
shell|-s      attach bash to the running container (docker exec), or start one
status        docker ps line for the container
pull          pull the docker image
help          this usage
Any other args are passed to codex directly, so $app with no option
starts the TUI and $app --model X works too.
Mounts: ~/.codex and the current directory.
EOF
}

case $1 in
    run|-r )
        shift
        run "$@"
        ;;
    exec|-e )
        shift
        exec_cmd "$@"
        ;;
    shell|-s )
        shell
        ;;
    status )
        status
        ;;
    pull )
        pull
        ;;
    help|-h )
        usage
        ;;
    * )
        run "$@"
        ;;
esac
