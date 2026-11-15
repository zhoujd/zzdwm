#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

FS=output/rootfs.tar.gz
IMG=custom-ai-agent:latest

load() {
    if [ -f ${FS} ]; then
        cat ${FS} | docker import - ${IMG}
        docker image prune --force 2>&1 >/dev/null
    fi
    echo "Run load image done"
}

shell() {
    docker run -it --rm ${IMG} sh
}

ai() {
    docker run -it --rm ${IMG} ai tui
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
    test|-t )
        docker run -it --rm ${IMG} ai --version
        docker run -it --rm ${IMG} me --version
        ;;
    * )
        echo "Usage $(basename $0) {load|-l|shell|-s|ai|-a|test|-t}"
        ;;
esac
