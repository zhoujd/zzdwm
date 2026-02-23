Utils
=====

## URLs

```
## https://github.com/altsem/gitu
```

## Blackwalls

```
## https://github.com/ericpruitt/edge/blob/master/utilities/blackwalls.c
```

## nq: queue utilities

```
## https://github.com/leahneukirchen/nq
```

## Busybox less

```
## Clone busybox
## https://github.com/mirror/busybox
## https://coral.googlesource.com/busybox
## https://git.busybox.net/busybox
$ git clone https://git.busybox.net/busybox
$ cd busybox

## Config for static less
## 1:Setting -> Build Options -> Build static library
## 2:Misc -> less
$ make allnoconfig
$ make menuconfig

## Build single less
$ ./make_single_applets.sh
$ ldd busybox_LESS
$ cp busybox_LESS less
```
