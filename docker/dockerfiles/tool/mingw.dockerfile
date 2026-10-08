# syntax=docker/dockerfile:1

ARG VARIANT=22.04
ARG PLATFORM=linux/amd64
FROM --platform=$PLATFORM ubuntu:$VARIANT

ARG MIRROR=mirrors.aliyun.com
RUN sed -i "s/archive.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list && \
    sed -i "s/security.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
    ca-certificates \
    make \
    gcc-mingw-w64-x86-64-posix \
    gcc-mingw-w64-i686-posix \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
