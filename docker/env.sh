#!/bin/sh
# Portable POSIX helper functions for Docker environment

getctnname() {
    prefix="${1:-container}"
    last_index=$(docker ps -a --format '{{.Names}}' 2>/dev/null \
        | grep -E "^${prefix}-[0-9]+$" \
        | sed "s/^${prefix}-//" \
        | sort -n \
        | tail -n 1)
    if [ -z "$last_index" ]; then
        echo "${prefix}-1"
    else
        echo "${prefix}-$((last_index + 1))"
    fi
}

cleanexit() {
    if command -v docker >/dev/null 2>&1; then
        docker container prune -f >/dev/null 2>&1 || {
            exited=$(docker ps -a -q -f "status=exited")
            [ -n "$exited" ] && echo "$exited" | xargs docker rm >/dev/null 2>&1
        }
    fi
}

cleannone() {
    if command -v docker >/dev/null 2>&1; then
        docker image prune -f >/dev/null 2>&1 || {
            dangling=$(docker images -f "dangling=true" -q)
            [ -n "$dangling" ] && echo "$dangling" | xargs docker rmi >/dev/null 2>&1
        }
    fi
}
