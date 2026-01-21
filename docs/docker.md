Docker
======

## Install Docker on Ubuntu 22.04

```bash
$ sudo apt install -y docker.io docker-buildx
```

## Alpine Images

```bash
## https://github.com/linuxcontainers/alpine
$ docker pull ghcr.io/linuxcontainers/alpine:3.20

```

## Void Linux Container Images

```bash
## https://github.com/void-linux/void-containers
REPOSITORY                              TAG      SIZE
ghcr.io/void-linux/void-glibc           latest   64.5MB
ghcr.io/void-linux/void-musl            latest   40.3MB
ghcr.io/void-linux/void-glibc-full      latest   135MB
ghcr.io/void-linux/void-musl-full       latest   81.4MB
ghcr.io/void-linux/void-glibc-busybox   latest   39.6MB
ghcr.io/void-linux/void-musl-busybox    latest   14.4MB

$ docker pull ghcr.io/void-linux/void-musl:latest
```

## Void Linux xbps

```
## https://github.com/void-linux/xbps
## https://docs.voidlinux.org/xbps/index.html
$ xbps-query -Rs <pkg>
$ xbps-install -Syu
$ xbps-install -Sy <pkg>
$ xbps-remove <pkg>
$ xbps-remove -R <pkg>
$ xbps-remove -Oo
```

## Void Linux Mirrors

```
## https://xmirror.voidlinux.org
## https://mirrors.bfsu.edu.cn/voidlinux/
## https://mirrors.tuna.tsinghua.edu.cn/voidlinux/
## https://mirror.sjtu.edu.cn/voidlinux/

## Changing Mirrors
$ sudo mkdir -p /etc/xbps.d
$ sudo cp /usr/share/xbps.d/*-repository-*.conf /etc/xbps.d/
$ REPO=https://mirrors.tuna.tsinghua.edu.cn/voidlinux
$ sudo sed -i "s|https://repo-default.voidlinux.org|$REPO|g" /etc/xbps.d/*-repository-*.conf

$ xbps-install -S
$ xbps-query -L
```

## Void Core Xorg & Driver

```
## 1. Update System
$ sudo xbps-install -S # Sync repos
$ sudo xbps-install -u # Update system

## 2. Install Xorg Server & Drivers
$ sudo xbps-install xorg-server mesa

## 3. Install Display Manager
sudo xbps-install gdm # For GNOME
# OR
sudo xbps-install lightdm # For LXQt/XFCE/etc.
```

## Void Configure Services

```
## 1. Enable D-Bus
$ sudo ln -s /etc/sv/dbus /var/service/

## 2. Enable Display Manager
$ sudo ln -s /etc/sv/gdm /var/service/ # For GDM
# OR
$ sudo ln -s /etc/sv/lightdm /var/service/ # For LightDM

## 3. Enable Network Manager (if using Wi-Fi/NetworkManager)
$ sudo ln -s /etc/sv/NetworkManager /var/service/
```
