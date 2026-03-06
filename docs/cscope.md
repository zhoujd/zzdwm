cscope
======

## URLs

```
## https://cscope.sourceforge.net/
```

## Static build with VoidLinux

```
$ cd docker
$ ./run -r -v
$ cd tools/utils/cscope
$ ./configure LDFLAGS="-static -s"
$ make -j4
$ file src/cscope
```
