FROM ghcr.io/void-linux/void-musl
RUN xbps-install -Sy make gcc libtool autoconf automake pkg-config \
        wget ncurses ncurses-devel file upx
USER root
CMD ["/bin/sh"]
