# syntax=docker/dockerfile:1

ARG VARIANT=20260101R1
FROM ghcr.io/void-linux/void-musl-full:$VARIANT

ARG REPO=https://mirrors.tuna.tsinghua.edu.cn/voidlinux
RUN mkdir -p /etc/xbps.d \
    && cp /usr/share/xbps.d/*-repository-*.conf /etc/xbps.d/ \
    && sed -i "s|https://repo-default.voidlinux.org|$REPO|g" \
    /etc/xbps.d/*-repository-*.conf

RUN xbps-install -Sy gcc make cmake libtool autoconf automake pkg-config \
    ncurses ncurses-devel wget file upx tar gzip bzip2 sudo shadow \
    openssh git findutils diffutils bash \
    && xbps-remove -Oo

ARG USER_NAME=zach
RUN useradd $USER_NAME -m \
    && usermod -aG wheel $USER_NAME \
    && echo $USER_NAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USER_NAME \
    && chmod 0440 /etc/sudoers.d/$USER_NAME

USER $USER_NAME

RUN cat > ~/.bashrc <<EOF
# .bashrc
alias ls='ls --color=auto'
TERM=xterm-256color
PS1='[\u@\h \W]\$ '
EOF

CMD ["/bin/bash"]
