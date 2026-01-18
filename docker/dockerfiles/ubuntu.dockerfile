# syntax=docker/dockerfile:1
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    musl-tools \
    python3-docutils \
    sudo \
    && rm -rf /var/lib/apt/lists/*

RUN useradd build -m \
    && echo build ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/build \
    && chmod 0440 /etc/sudoers.d/build

USER build
