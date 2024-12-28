#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

BUILD_FILE=$SCRIPT_ROOT/dockerfiles/alpine.dockerfile
BUILD_ARG=
IMG=zz/alpine
TAG=base
CNT=zz-build-1

case $1 in
    build|-b )
        docker build $BUILD_ARG -t $IMG:$TAG -f $BUILD_FILE .
        docker tag $IMG:$TAG $IMG:latest
        ;;
    run|-r )
        docker stop $CNT 2>/dev/null
        docker rm $CNT 2>/dev/null
        docker run -it --name $CNT $IMG:$TAG
        ;;
    status|-s )
        docker ps | grep $IMG
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
