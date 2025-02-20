#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

IMG=zhoujd/alpine
#IMG=zhoujd/void-linux
TAG=base
CNT=zz-build-1

case $1 in
    build|-b )
        make -C dockerfiles
        ;;
    run|-r )
        docker stop $CNT >/dev/null 2>&1
        docker rm $CNT >/dev/null 2>&1
        docker run -it --name $CNT $IMG:$TAG
        ;;
    status|-s )
        echo "IMG:"
        docker images | grep $IMG
        echo "PS:"
        docker ps -a | grep $IMG
        ;;
    clean|-c )
        docker system prune -f
        ;;
    purge )
        docker system prune -f -a
        ;;
    * )
        echo "Usage: $(basename $0) {build|-b|run|-r|status|-s|clean|-c|purge}"
        ;;
esac
