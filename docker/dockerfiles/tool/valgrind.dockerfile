ARG VARIANT=22.04
ARG PLATFORM=linux/amd64
FROM --platform=$PLATFORM ubuntu:$VARIANT

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive
ENV TERM=linux

ARG MIRROR=mirrors.aliyun.com
RUN sed -i "s/archive.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list && \
    sed -i "s/security.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list

# Install prerequisites
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
    build-essential musl-tools sudo git wget \
    python3-pip python3-venv python3-docutils \
    && rm -f /tmp/*.deb \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Config PIP
ARG PIP_URL=https://pypi.tuna.tsinghua.edu.cn/simple
RUN pip3 config set global.index-url ${PIP_URL}

# Install build dependencies, Valgrind, and debug symbols
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    valgrind \
    libc6-dbg \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
