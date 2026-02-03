# syntax=docker/dockerfile:1

ARG VARIANT=3.20
ARG PLATFORM=linux/amd64
FROM --platform=$PLATFORM ghcr.io/linuxcontainers/alpine:$VARIANT

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
ARG USER_SHELL=/bin/bash
RUN adduser -D -s $USER_SHELL $USER_NAME \
    && addgroup $USER_NAME shadow \
    && addgroup $USER_NAME wheel \
    && echo $USER_NAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USER_NAME \
    && chmod 0440 /etc/sudoers.d/$USER_NAME

USER $USER_NAME

RUN cat > ~/.profile <<EOF
# .profile
[ -f ~/.bashrc ] && . ~/.bashrc
EOF

RUN cat > ~/.bashrc <<EOF
# .bashrc
alias ls='ls --color=auto'
TERM=xterm-256color
PS1='[\u@\h \W]\$ '
EOF

# Setup entrypoint
COPY entrypoint.sh /
ENTRYPOINT ["/entrypoint.sh"]
CMD ["run"]
