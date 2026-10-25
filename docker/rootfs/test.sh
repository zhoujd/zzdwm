#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

FS=output/rootfs.tar.gz

load() {
    if [ -f ${FS} ]; then
        cat ${FS} | docker import - custom-ai-agent:latest
        docker image prune --force 2>&1 >/dev/null
    fi
    echo "Run load image done"
}

shell() {
    docker run -it --rm custom-ai-agent:latest sh
}

ai() {
    docker run -it --rm custom-ai-agent:latest ai tui
}

case $1 in
    load|-l )
        shift
        load "$@"
        ;;
    shell|-s )
        shift
        shell "$@"
        ;;
    ai|-a )
        shift
        ai "$@"
        ;;
    * )
        echo "Usage $(basename $0) {load|-l|shell|-s|ai|-a}"
        ;;
esac
