#!/bin/bash

SCRIPT_ROOT=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
TOP_ROOT=$(cd $SCRIPT_ROOT/.. && pwd)
CNT=zz-build-1

run() {
    local kind=$1
    local img=zhoujd/alpine
    local tag=base
    case $kind in
        alpine|-a )
            img=zhoujd/alpine
            ;;
        void|-v )
            img=zhoujd/void-linux
            ;;
    esac
    docker stop $CNT >/dev/null 2>&1
    docker rm $CNT >/dev/null 2>&1
    docker run -it --name $CNT \
       -v $TOP_ROOT:/root/$(basename $TOP_ROOT) \
       $img:$tag
    echo "Run Done"
}

status() {
    echo "IMG:"
    docker images | grep zhoujd/alpine
    docker images | grep zhoujd/void-linux
    echo "PS:"
    docker ps -a | grep $CNT
    echo "Status Done"
}

clean() {
    docker stop $CNT >/dev/null 2>&1
    docker rm $CNT >/dev/null 2>&1
    echo "Clean Done"
}

usage() {
    local app=$(basename $0)
    cat <<EOF
Usage: $app {build|-b|run|-r|status|-s|clean|-c|purge}
run|-r   --   {alpine|-a|void|-v}
EOF
}

case $1 in
    build|-b )
        make -C dockerfiles
        ;;
    run|-r )
        shift
        run "$@"
        ;;
    status|-s )
        status
        ;;
    clean|-c )
        clean
        ;;
    purge )
        docker system prune -f -a
        ;;
    * )
        usage
        ;;
esac
