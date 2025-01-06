dtach
=====


## URLs

```
https://github.com/crigler/dtach
```

## Build dtach having detach feature of screen -static

```
$ git clone https://github.com/crigler/dtach
$ cd dtach
$ configure
$ make LDFLAGS=-static
$ ./detach --help
```

## Usage guide

```
## Create a new session
$ dtach -A /tmp/z-0 bash -l

## Attach sesion
$ dtach -a /tmp/z-0
```
