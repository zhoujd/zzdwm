ARG VARIANT=22.04
ARG PLATFORM=linux/amd64
FROM --platform=$PLATFORM ubuntu:$VARIANT

ARG MIRROR=mirrors.aliyun.com
RUN sed -i "s/archive.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list && \
    sed -i "s/security.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list

# Install prerequisites
RUN apt-get update \
    && DEBIAN_FRONTEND="noninteractive" apt-get install -y --no-install-recommends \
        apt-transport-https \
        ca-certificates \
        cabextract \
        git \
        gnupg \
        gosu \
        gpg-agent \
        locales \
        p7zip \
        pulseaudio \
        pulseaudio-utils \
        sudo \
        tzdata \
        unzip \
        wget \
        winbind \
        xvfb \
        zenity \
    && rm -rf /var/lib/apt/lists/*

# Install wine
ARG WINE_BRANCH="stable"
RUN wget -nv -O- https://dl.winehq.org/wine-builds/winehq.key | APT_KEY_DONT_WARN_ON_DANGEROUS_USAGE=1 apt-key add - \
    && echo "deb https://dl.winehq.org/wine-builds/ubuntu/ $(grep VERSION_CODENAME= /etc/os-release | cut -d= -f2) main" >> /etc/apt/sources.list \
    && dpkg --add-architecture i386 \
    && apt-get update \
    && DEBIAN_FRONTEND="noninteractive" apt-get install -y --install-recommends winehq-${WINE_BRANCH} \
    && rm -rf /var/lib/apt/lists/*

# Install winetricks
RUN wget -nv -O /usr/bin/winetricks https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks \
    && chmod +x /usr/bin/winetricks

# Setup entrypoint
COPY entrypoint.sh /
ENTRYPOINT ["/entrypoint.sh"]
