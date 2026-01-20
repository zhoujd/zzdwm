FROM ghcr.io/void-linux/void-musl-full:20260101R1

RUN xbps-install -Sy gcc make cmake libtool autoconf automake pkg-config \
        ncurses ncurses-devel wget file upx tar gzip bzip2 sudo \
        openssh git findutils diffutils bash \
        && xbps-remove -Oo

ARG USER_NAME=zach
RUN useradd $USER_NAME -m \
    && echo $USER_NAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USER_NAME \
    && chmod 0440 /etc/sudoers.d/$USER_NAME

USER $USER_NAME
RUN sed -i s/PS1.*// ~/.bashrc
RUN cat >> ~/.bashrc <<EOF
# some more ls aliases
alias ll='ls -alF'
alias la='ls -A'
alias l='ls -CF'

# custom PS1
PS1='\w \$ '
EOF

CMD ["/bin/bash"]
