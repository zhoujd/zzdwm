FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    musl-tools \
    python3-docutils \
    sudo \
    && rm -rf /var/lib/apt/lists/*

ARG USER_NAME=zach
RUN useradd $USER_NAME -m \
    && echo $USER_NAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USER_NAME \
    && chmod 0440 /etc/sudoers.d/$USER_NAME

USER $USER_NAME
RUN cat >> ~/.bashrc <<EOF
export PS1='\w \$ '
EOF
