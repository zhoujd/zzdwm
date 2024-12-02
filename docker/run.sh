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
        docker run -it --name $CNT_NAME $CNT_IMG
        ;;
    status|st )
        docker ps | grep $CNT_IMG
        ;;
    * )
        echo "Usage: $(basename $0) {build|-b|run|-r|status|st}"
        ;;
esac
