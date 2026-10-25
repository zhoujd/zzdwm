#!/bin/bash

if [ -f output/rootfs.tar.gz ]; then
    cat output/rootfs.tar.gz | docker import - custom-ai-agent:latest 2>&1 >/dev/null
    docker image prune --force 2>&1 >/dev/null
fi

case $1 in
    shell|-s )
        docker run -it --rm custom-ai-agent:latest sh
        ;;
    ai|-a )
        docker run -it --rm custom-ai-agent:latest ai tui
        ;;
    * )
        echo "Usage $(basename $0) {shell|-s|ai|-a}"
        ;;
esac
