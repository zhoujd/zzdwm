## env.sh

getctnname() {
    CONTAINER_PREFIX=$1
    LAST_INDEX=$(docker ps -a --format "{{.Names}}" \
      | grep "^${CONTAINER_PREFIX}-" \
      | sed "s/^${CONTAINER_PREFIX}-//" \
      | sort -n | tail -n 1)

    # Default to 0 if no container exists
    if [ -z "$LAST_INDEX" ]; then
	NEXT_INDEX=1
    else
	NEXT_INDEX=$((LAST_INDEX + 1))
    fi
    echo "${CONTAINER_PREFIX}-${NEXT_INDEX}"
}

cleanexit() {
    ps_list=$(docker ps -a | grep Exit )
    if [ -n "$ps_list" ]; then
        docker ps -a | grep Exit | cut -d ' ' -f 1 | xargs docker rm
    fi
}

cleannone() {
    img_list=$(docker images --filter "dangling=true" -q --no-trunc)
    if [ -n "$img_list" ]; then
        docker rmi $img_list
    fi
}
