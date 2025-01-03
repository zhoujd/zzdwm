tig
===

## Build tig with -static

```bash
# git clone https://github.com/jonas/tig.git
# apk add automake autoconf ncurses-static ncurses-dev libncursesw
# ./autogen.sh
# ./configure LDFLAGS=-static
# make
# ls -l src/tig
```
