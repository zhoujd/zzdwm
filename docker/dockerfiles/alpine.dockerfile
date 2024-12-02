FROM ghcr.io/linuxcontainers/alpine:3.20
RUN /sbin/apk update --no-cache \
        && /sbin/apk upgrade --no-cache \
        && /sbin/apk add git libbsd-static libbsd-dev \
        ncurses-dev musl-dev ncurses-static gcc make \
        && /bin/rm -rf /var/cache/apk/*
CMD ["/bin/sh"]
