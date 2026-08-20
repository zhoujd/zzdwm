# syntax=docker/dockerfile:1

ARG VARIANT=22.04
FROM ubuntu:${VARIANT}

# Mirror selection for fast package downloads
ARG MIRROR=mirrors.aliyun.com
RUN sed -i "s/archive.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list* 2>/dev/null || true && \
    sed -i "s/security.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list* 2>/dev/null || true

# Install base toolchain & clean up cache layers
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        musl-tools \
        sudo \
        git \
        python3-pip \
        python3-venv \
        python3-docutils && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/*

# Copy tools to system path
COPY tools /usr/local/bin

# Resolve /var/run symlink and setup utmp logging
RUN rm -rf /var/run && \
    mkdir -p /var/run && \
    touch /var/run/utmp

# Git global directory trust override
RUN git config --global --add safe.directory '*'

# User creation and sudo privileges
ARG USER_NAME=zach
ARG USER_SHELL=/bin/bash
RUN useradd ${USER_NAME} -m -s ${USER_SHELL} && \
    usermod -aG sudo,shadow ${USER_NAME} && \
    echo "${USER_NAME} ALL=(root) NOPASSWD:ALL" > /etc/sudoers.d/${USER_NAME} && \
    chmod 0440 /etc/sudoers.d/${USER_NAME}

USER ${USER_NAME}

# Python pip mirror configuration
ARG PIP_URL=https://pypi.tuna.tsinghua.edu.cn/simple
RUN pip3 config set global.index-url ${PIP_URL} && \
    pip3 config set global.break-system-packages true 2>/dev/null || true

# User shell profile customization
RUN echo "alias ls='ls --color=auto'" >> ~/.bashrc && \
    echo "export TERM=xterm-256color" >> ~/.bashrc && \
    echo "export PS1='[\u@\h \W]\$ '" >> ~/.bashrc

# Entrypoint setup with correct user permissions
COPY --chown=${USER_NAME}:${USER_NAME} entrypoint.sh /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
