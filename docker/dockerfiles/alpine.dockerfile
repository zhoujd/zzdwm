FROM ghcr.io/linuxcontainers/alpine:latest
USER root
RUN apk update --no-cache \
        && apk upgrade --no-cache \
        && apk add \
        libbsd-static libbsd-dev ncurses-dev musl-dev ncurses-static \
        gcc make libtool autoconf automake \
        git openssh \
        && rm -rf /var/cache/apk/*
ENV PS1="\w \$ "
CMD ["/bin/sh"]
