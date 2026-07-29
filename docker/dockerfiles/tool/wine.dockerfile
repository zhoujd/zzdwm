ARG VARIANT=22.04
ARG PLATFORM=linux/amd64
FROM --platform=$PLATFORM ubuntu:$VARIANT

ARG MIRROR=mirrors.aliyun.com
RUN sed -i "s/archive.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list && \
    sed -i "s/security.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list

# Install prerequisites
RUN apt-get update \
    && DEBIAN_FRONTEND="noninteractive" apt-get install -y --no-install-recommends \
        wget \
    && rm -rf /var/lib/apt/lists/*

# Install wine
RUN dpkg --add-architecture i386 \
    && apt-get update \
    && DEBIAN_FRONTEND="noninteractive" apt-get install -y --install-recommends \
        wine wine32 \
        libvulkan1 libvulkan1:i386 \
        mesa-vulkan-drivers mesa-vulkan-drivers:i386 \
    && rm -rf /var/lib/apt/lists/*

# Install winetricks
ARG WINETRICKS_URL=https://raw.githubusercontent.com/Winetricks/winetricks/master/src/winetricks
RUN wget -nv -O /usr/bin/winetricks ${WINETRICKS_URL} \
    && chmod +x /usr/bin/winetricks
