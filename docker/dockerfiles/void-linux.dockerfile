FROM ghcr.io/void-linux/void-musl:latest
USER root
RUN xbps-install -Sy make gcc libtool autoconf automake pkg-config \
        wget ncurses ncurses-devel file upx tar gzip bzip2 \
        openssh git findutils diffutils \
        && xbps-remove -Oo
CMD ["/bin/sh"]
