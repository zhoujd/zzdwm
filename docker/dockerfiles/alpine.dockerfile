FROM ghcr.io/linuxcontainers/alpine:latest
RUN /sbin/apk update --no-cache \
    && /sbin/apk upgrade --no-cache \
    && /bin/rm -rf /var/cache/apk/*
RUN apk add git libbsd-static libbsd-dev ncurses-dev musl-dev ncurses-static gcc make
CMD ["/bin/sh"]
