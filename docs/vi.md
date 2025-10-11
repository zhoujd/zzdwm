vi
==

## Build busybox vi

```
## Clone busybox
## https://github.com/mirror/busybox
## https://coral.googlesource.com/busybox
$ git clone https://github.com/mirror/busybox
$ cd busybox

## Config for static VI
## 1:setting->static build
## 2:editor->vi
$ make allnoconfig
$ make menuconfig

## Build single VI
$ ./make_single_applets.sh
$ ldd busybox_VI
$ cp busybox_VI vi
```

## Tiny viless like vi

```
## https://github.com/brentr/viless
## https://github.com/RaymiiOrg/viless
## https://raymii.org/s/blog/Bare_Metal_Boot_to_Vi.html
```
