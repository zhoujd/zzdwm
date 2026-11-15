#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
TOP=$(cd $SCRIPT_ROOT/../.. && pwd)

CTN_NAME=claude-code
CTN_USER=zach
IMG=zhoujd/claude:latest

MNT=/home/$CTN_USER/work
WS=$MNT/$(basename $PWD)

## The host settings.json may not carry ANTHROPIC_API_KEY (the key is only
## exported into the shell by ~/.claude/.env on some hosts); pass it
## through explicitly so the container is always authenticated.
API_KEY=$(sed -n 's/^export ANTHROPIC_API_KEY=//p' \
            $HOME/.claude/.env 2>/dev/null)

RUN_PARAM=(
    --rm
    --name=$CTN_NAME
    --user=$CTN_USER
    --network=host
    -e TERM=${TERM:-xterm-256color}
    -v $HOME/.claude:/home/$CTN_USER/.claude
    -v $HOME/.claude.json:/home/$CTN_USER/.claude.json
    -v $PWD:$WS
    -w $WS
)

[ -n "$API_KEY" ] && RUN_PARAM+=(-e ANTHROPIC_API_KEY=$API_KEY)

check() {
    if ! docker image inspect $IMG >/dev/null 2>&1; then
        echo "Missing $IMG; build it first:" >&2
        echo "    make -C $TOP/docker/dockerfiles/tool claude-base" >&2
        exit 1
    fi
    ## The image user is uid 1000; files created in the mounted workspace
    ## belong to that uid on the host.
    if [ "$(id -u)" != 1000 ]; then
        echo "Warning: host uid $(id -u) != 1000, mounted files will be owned by 1000" >&2
    fi
}

run() {
    check
    docker run -it ${RUN_PARAM[@]} $IMG claude "$@"
}

print() {
    check
    docker run ${RUN_PARAM[@]} $IMG claude -p "$@"
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
run|-r        interactive claude in a container [claude args]
print|-p      headless print mode, e.g. $app -p "say ok"
shell|-s      attach bash to the running container (docker exec), or start one
status        docker ps line for the container
pull          pull the docker image
help          this usage
Any other args are passed to claude directly, so $app with no option
starts the TUI and $app --model X works too.
Mounts: ~/.claude, ~/.claude.json and the current directory.
EOF
}

case $1 in
    run|-r )
        shift
        run "$@"
        ;;
    print|-p )
        shift
        print "$@"
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
