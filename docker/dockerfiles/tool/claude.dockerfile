ARG VARIANT=22.04
ARG PLATFORM=linux/amd64
FROM --platform=$PLATFORM ubuntu:$VARIANT

ARG MIRROR=mirrors.aliyun.com
RUN sed -i "s/archive.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list && \
    sed -i "s/security.ubuntu.com/${MIRROR}/g" /etc/apt/sources.list

ARG NODE_VERSION=v22.23.5
ARG CLAUDE_VERSION=latest

# Node >= 18 from the official tarball; the distro node (12.x on Ubuntu
# 22.04) is too old for Claude Code.
RUN ARCH=$(uname -m | sed 's/^x86_64$/x64/;s/^aarch64$/arm64/') \
    && curl -fsSL https://nodejs.org/dist/${NODE_VERSION}/node-${NODE_VERSION}-linux-${ARCH}.tar.xz \
        | tar -xJ -C /usr/local --strip-components=1 \
    && node --version \
    && npm --version

RUN npm install -g @anthropic-ai/claude-code@${CLAUDE_VERSION} \
    && claude --version \
    && npm cache clean --force

ARG USER_NAME=zach
ARG USER_HOME=/home/$USER_NAME
ARG USER_SHELL=/bin/bash
RUN useradd -d "$USER_HOME" -s "$USER_SHELL" -m "$USER_NAME" \
    && adduser "$USER_NAME" sudo \
    && echo "$USER_NAME ALL=(ALL) NOPASSWD:ALL" > "/etc/sudoers.d/$USER_NAME"

COPY claude-entrypoint.sh /app/entrypoint.sh
RUN chmod 0755 /app/entrypoint.sh

WORKDIR $USER_HOME
USER $USER_NAME
ENV HOME=$USER_HOME \
    USER=$USER_NAME \
    SHELL=$USER_SHELL \
    PATH=$USER_HOME/.local/bin:$PATH \
    TERM=xterm-256color

ENTRYPOINT ["/app/entrypoint.sh"]
CMD ["claude"]
