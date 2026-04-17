# syntax=docker/dockerfile:1

ARG VARIANT=20260101R1
ARG PLATFORM=linux/amd64
FROM --platform=$PLATFORM ghcr.io/void-linux/void-musl-full:$VARIANT

ARG MIRROR=https://mirrors.tuna.tsinghua.edu.cn/voidlinux
RUN mkdir -p /etc/xbps.d \
    && cp /usr/share/xbps.d/*-repository-*.conf /etc/xbps.d/ \
    && sed -i "s|https://repo-default.voidlinux.org|${MIRROR}|g" \
    /etc/xbps.d/*-repository-*.conf

RUN xbps-install -Syu xbps \
    && xbps-install -Sy gcc make cmake libtool autoconf automake pkg-config \
    ncurses ncurses-devel wget file upx tar gzip bzip2 sudo shadow \
    openssh git findutils diffutils bash python3 python3-pip \
    && xbps-remove -Oo

ARG USER_NAME=zach
RUN useradd $USER_NAME -m \
    && usermod -aG wheel $USER_NAME \
    && echo $USER_NAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USER_NAME \
    && chmod 0440 /etc/sudoers.d/$USER_NAME

USER $USER_NAME

ARG PIP_URL=https://pypi.tuna.tsinghua.edu.cn/simple
RUN pip3 config set global.index-url ${PIP_URL}

RUN cat > ~/.bashrc <<EOF
# .bashrc
alias ls='ls --color=auto'
TERM=xterm-256color
PS1='[\u@\h \W]\$ '
EOF

# Setup entrypoint
COPY entrypoint.sh /
ENTRYPOINT ["/entrypoint.sh"]
