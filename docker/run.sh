#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

BUILD_FILE=$SCRIPT_ROOT/dockerfiles/alpine.dockerfile
BUILD_ARG=
CNT_IMG=zz/alpine:base
CNT_NAME=zz-build-1

case $1 in
    build|-b )
        docker build $BUILD_ARG -t $CNT_IMG -f $BUILD_FILE .
        ;;
    run|-r )
        docker stop $CNT_NAME 2>/dev/null
        docker rm $CNT_NAME 2>/dev/null
        docker run -it --name $CNT_NAME $CNT_IMG
        ;;
    status|-s )
        docker ps | grep $CNT_IMG
        ;;
    clean|-c )
        docker system prune -f
        ;;
    * )
        echo "Usage: $(basename $0) {build|-b|run|-r|status|-s|clean|-c}"
        ;;
esac
