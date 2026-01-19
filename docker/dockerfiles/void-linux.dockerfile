FROM ghcr.io/void-linux/void-musl:20260101R1
USER root
RUN xbps-install -Sy make gcc libtool autoconf automake pkg-config \
        ncurses ncurses-devel wget file upx tar gzip bzip2 \
        openssh git findutils diffutils bash \
        && xbps-remove -Oo
ENV PS1="\w \$ "
CMD ["/bin/bash"]
