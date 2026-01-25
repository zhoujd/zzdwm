# syntax=docker/dockerfile:1
ARG VARIANT=3.20
FROM ghcr.io/linuxcontainers/alpine:3.20

ARG VARIANT
RUN cat > /etc/apk/repositories <<EOF
https://mirrors.tuna.tsinghua.edu.cn/alpine/v$VARIANT/main
https://mirrors.tuna.tsinghua.edu.cn/alpine/v$VARIANT/community
EOF

RUN apk update --no-cache \
    && apk upgrade --no-cache \
    && apk add \
    libbsd-static libbsd-dev ncurses-dev musl-dev ncurses-static \
    gcc make libtool autoconf automake \
    git bash sudo shadow openssh \
    && rm -rf /var/cache/apk/*

ARG USER_NAME=zach
RUN useradd $USER_NAME -m \
    && echo $USER_NAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USER_NAME \
    && chmod 0440 /etc/sudoers.d/$USER_NAME

USER $USER_NAME
RUN cat > ~/.bashrc <<EOF
# .bashrc
alias ls='ls --color=auto'
PS1="\w \$ "
EOF

CMD ["/bin/bash"]
