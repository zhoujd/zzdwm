PROG = cwm
OBJS = calmwm.o client.o conf.o group.o kbfunc.o menu.o parse.o \
	   reallocarray.o screen.o search.o strlcat.o strlcpy.o strtonum.o \
	   util.o xevents.o xmalloc.o xutil.o
LIBS = x11 xft xrandr

PREFIX ?= /usr/local
BINDIR ?= ${PREFIX}/bin
MANDIR ?= ${PREFIX}/share/man

PKG_CONFIG ?= pkg-config

CFLAGS   ?= -O2
CFLAGS   += -MMD -Wall -Wextra -Wno-unused-parameter
CPPFLAGS += -D_GNU_SOURCE $(shell ${PKG_CONFIG} --cflags ${LIBS})
LDLIBS   += $(shell ${PKG_CONFIG} --libs ${LIBS})

${PROG}: ${OBJS}
	${CC} -o $@ ${CFLAGS} ${OBJS} ${LDLIBS}

install:
	install -D -m 0755 -t '${DESTDIR}${BINDIR}' ${PROG}
	install -D -m 0644 -t '${DESTDIR}${MANDIR}/man1' ${PROG}.1
	install -D -m 0644 -t '${DESTDIR}${MANDIR}/man5' ${PROG}rc.5

clean:
	rm -f ${OBJS} ${OBJS:.o=.d} ${PROG}

.INTERMEDIATE: parse.c
.PHONY: clean install

-include ${OBJS:.o=.d}
