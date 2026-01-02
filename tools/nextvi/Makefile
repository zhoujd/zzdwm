## Makefile

all: vi

CFLAGS := \
       -static \
       -pedantic -Wall -Wextra \
       -Wno-implicit-fallthrough \
       -Wno-missing-field-initializers \
       -Wno-unused-parameter \
       -Wfatal-errors \
       ${CFLAGS}

CFLAGS += -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
CFLAGS += -MMD

CC = c99

PREFIX = /usr/local

install: strip
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f vi ${DESTDIR}${PREFIX}/bin/vi

strip: vi
	strip -v vi

-include vi.d
vi: vi.c
	${CC} ${CFLAGS} -o $@ vi.c

clean:
	-rm -f vi *.d *.o
