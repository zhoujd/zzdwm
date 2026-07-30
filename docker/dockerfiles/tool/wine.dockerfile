ARG VARIANT=22.04
ARG PLATFORM=linux/amd64
FROM --platform=$PLATFORM ubuntu:$VARIANT

ARG MIRROR=mirrors.aliyun.com
RUN sed -i "s/archive.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list && \
    sed -i "s/security.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list

# Enable 32-bit architecture and install standard Wine components
RUN dpkg --add-architecture i386 \
    && apt-get update \
    && DEBIAN_FRONTEND="noninteractive" apt-get install -y --no-install-recommends \
    ca-certificates \
    wine \
    wine64 \
    wine32 \
    && rm -rf /var/lib/apt/lists/*

# Suppress Wine GUI popups for a cleaner terminal test
ENV WINEDEBUG=-all
ENV WINEPREFIX=/root/.wine

WORKDIR /app
