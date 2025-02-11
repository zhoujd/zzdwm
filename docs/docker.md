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
