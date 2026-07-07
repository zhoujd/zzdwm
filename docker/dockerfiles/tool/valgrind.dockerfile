ARG VARIANT=22.04
ARG PLATFORM=linux/amd64
FROM --platform=$PLATFORM ubuntu:$VARIANT

ARG MIRROR=mirrors.aliyun.com
RUN sed -i "s/archive.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list && \
    sed -i "s/security.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive
ENV TERM=linux

# Install build dependencies, Valgrind, and debug symbols
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    valgrind \
    libc6-dbg \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
