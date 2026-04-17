# syntax=docker/dockerfile:1

ARG VARIANT=22.04
ARG PLATFORM=linux/amd64
FROM --platform=$PLATFORM ubuntu:$VARIANT

ARG MIRROR=mirrors.aliyun.com
RUN sed -i "s/archive.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list && \
    sed -i "s/security.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential musl-tools sudo git \
    python3-pip python3-venv python3-docutils \
    && rm -rf /var/lib/apt/lists/*

ARG USER_NAME=zach
ARG USER_SHELL=/bin/bash
RUN useradd $USER_NAME -m -s $USER_SHELL \
    && usermod -aG sudo,shadow $USER_NAME \
    && echo $USER_NAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USER_NAME \
    && chmod 0440 /etc/sudoers.d/$USER_NAME

USER $USER_NAME

ARG PIP_URL=https://pypi.tuna.tsinghua.edu.cn/simple
RUN pip3 config set global.index-url ${PIP_URL}

RUN cat > ~/.bashrc <<EOF
# .bashrc

# If not running interactively, don't do anything
[[ \$- != *i* ]] && return

alias ls='ls --color=auto'
TERM=xterm-256color
PS1='[\u@\h \W]\$ '
EOF

# Setup entrypoint
COPY entrypoint.sh /
ENTRYPOINT ["/entrypoint.sh"]
