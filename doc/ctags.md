ctags
=====

## URLs

```
## https://ctags.io
## https://github.com/universal-ctags/ctags
## https://docs.ctags.io/en/latest/index.html
## https://linux.die.net/man/1/ctags
```

## Static build VoidLinux

```
$ cd docker
$ ./run -r -v
$ git clone https://github.com/universal-ctags/ctags.git
$ cd ctags
$ ./autogen.sh
$ ./configure LDFLAGS="-static -s"
$ make -j4
```
