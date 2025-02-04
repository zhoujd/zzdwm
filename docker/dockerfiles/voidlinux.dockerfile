FROM voidlinux/voidlinux-musl

RUN xbps-install -Sy make gcc libtool autoconf automake pkg-config wget ncurses ncurses-devel sudo file upx

ENV user core

RUN useradd -d /home/$user -m -s /bin/bash $user
RUN echo "$user ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$user
RUN chmod 0440 /etc/sudoers.d/$user

USER $user
