mingw
=====

## Install mingw

```
$ sudo apt install mingw-w64
$ sudo apt install mingw-w64-tools
$ sudo apt install mingw-w64-x86-64-dev
or
$ sudo apt install g++-mingw-w64-x86-64 gcc-mingw-w64-x86-64 # For MSVCRT
$ sudo apt install g++-mingw-w64-ucrt64 gcc-mingw-w64-ucrt64 # For UCRT
```

## Build hello

```
$ cat hello.c <<EOF
// hello.c
#include <stdio.h>

int main(void) {
    printf("Hello, Windows!\n");
    return 0;
}
EOF
$ x86_64-w64-mingw32-gcc hello.c -o hello.exe     # For MSVCRT target
$ x86_64-w64-mingw32ucrt-gcc hello.c -o hello.exe # For UCRT target
```

## Makefile with mingw

```
CC = x86_64-w64-mingw32-gcc
DEBUG = yes
ifeq ($(DEBUG), yes)
CFLAGS = -g -O0 -DLINUX $(INC) -Wall
else
CFLAGS = -Os -s -DLINUX $(INC) -Wall
endif

LD = x86_64-w64-mingw32-ld
LDFLAGS = -L/usr/x86_64-w64-mingw32/lib -static
LIBS = -lmingwex -lucrt -lmsvcrt -lmingw32 -lmingwthrd -lkernel32 -lmstask
```
