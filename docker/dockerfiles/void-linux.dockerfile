FROM ghcr.io/void-linux/void-musl
RUN xbps-install -Sy make gcc libtool autoconf automake pkg-config \
        wget ncurses ncurses-devel file upx tar gzip bzip2
USER root
CMD ["/bin/sh"]
