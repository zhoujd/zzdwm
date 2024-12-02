#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

BUILD_FILE=$SCRIPT_ROOT/dockerfiles/alpine.dockerfile
BUILD_ARG=
IMG=zz/alpine:base

case $1 in
    build|-b )
        docker build $BUILD_ARG -t $IMG -f $BUILD_FILE .
        ;;
    run|-r )
        docker run -it $IMG
        ;;
    status|st )
        docker ps | grep $IMG
        ;;
    * )
        echo "Usage: $(basename $0) {build|-b|run|-r|status|st}"
        ;;
esac
