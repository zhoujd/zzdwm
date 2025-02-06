FROM ghcr.io/linuxcontainers/alpine:3.20
RUN apk update --no-cache \
        && apk upgrade --no-cache \
        && apk add \
        libbsd-static libbsd-dev ncurses-dev musl-dev ncurses-static \
        git gcc make openssh \
        && rm -rf /var/cache/apk/*
USER root
CMD ["/bin/sh"]
