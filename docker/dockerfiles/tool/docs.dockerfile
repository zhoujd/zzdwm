# syntax=docker/dockerfile:1

ARG VARIANT=22.04
ARG PLATFORM=linux/amd64
FROM --platform=$PLATFORM ubuntu:$VARIANT

ARG MIRROR=mirrors.aliyun.com
RUN sed -i "s/archive.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list && \
    sed -i "s/security.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list

# Install the Pandoc and LaTeX toolchain used to build the MicroEMACS manual.
RUN apt-get update \
    && DEBIAN_FRONTEND="noninteractive" apt-get install -y --no-install-recommends \
    ca-certificates \
    make \
    pandoc \
    texlive-latex-extra \
    texlive-fonts-recommended \
    lmodern \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
