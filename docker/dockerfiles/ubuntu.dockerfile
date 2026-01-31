# syntax=docker/dockerfile:1

ARG VARIANT=22.04
FROM ubuntu:$VARIANT

ARG MIRROR=mirrors.aliyun.com
RUN sed -i "s/archive.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list && \
    sed -i "s/security.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list

RUN apt-get update && apt-get install -y \
    build-essential musl-tools \
    python3-docutils \
    sudo git \
    && rm -rf /var/lib/apt/lists/*

ARG USER_NAME=zach
RUN useradd $USER_NAME -m \
    && usermod -aG sudo,shadow $USER_NAME \
    && echo $USER_NAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USER_NAME \
    && chmod 0440 /etc/sudoers.d/$USER_NAME

USER $USER_NAME
RUN cat > ~/.bashrc <<EOF
# .bashrc

# If not running interactively, don't do anything
[[ \$- != *i* ]] && return

alias ls='ls --color=auto'
PS1='[\u@\h \W]\$ '
EOF
