ARG VARIANT=22.04
ARG PLATFORM=linux/amd64
FROM --platform=$PLATFORM ubuntu:$VARIANT

ARG MIRROR=mirrors.aliyun.com
RUN sed -i "s/archive.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list && \
    sed -i "s/security.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list

# Install the 64-bit Wine runtime used to test the MinGW me.exe build.
RUN apt-get update \
    && DEBIAN_FRONTEND="noninteractive" apt-get install -y --no-install-recommends \
    ca-certificates \
    wine64 \
    xvfb \
    && rm -rf /var/lib/apt/lists/*

RUN ln -s /usr/bin/wine64-stable /usr/local/bin/wine64

# Suppress Wine GUI popups and optional runtime downloads.
ENV WINEDEBUG=-all
ENV WINEDLLOVERRIDES=mscoree,mshtml=
ENV WINEPREFIX=/root/.wine

# Initialize the Wine prefix once so test containers start quickly.
RUN wine64 wineboot.exe -i

WORKDIR /app
