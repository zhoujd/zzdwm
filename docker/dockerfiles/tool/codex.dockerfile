# syntax=docker/dockerfile:1

ARG VARIANT=latest
FROM zhoujd/ubuntu:${VARIANT}

USER root

# Install base toolchain & clean up cache layers
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        bubblewrap \
        ca-certificates \
        curl \
        file \
        git \
        jq \
        ripgrep \
        tar \
        unzip \
        wget && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/* /tmp/*

ARG NODE_VERSION=v22.23.5
ARG CODEX_VERSION=latest

# Node >= 16 from the official tarball;
# the distro node (12.x on Ubuntu 22.04) is too old for Codex CLI.
RUN ARCH=$(uname -m | sed 's/^x86_64$/x64/;s/^aarch64$/arm64/') \
    && curl -fsSL https://nodejs.org/dist/${NODE_VERSION}/node-${NODE_VERSION}-linux-${ARCH}.tar.xz \
        | tar -xJ -C /usr/local --strip-components=1 \
    && node --version \
    && npm --version

RUN npm install -g @openai/codex@${CODEX_VERSION} \
    && codex --version \
    && npm cache clean --force

COPY codex-entrypoint.sh /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
CMD ["codex"]
