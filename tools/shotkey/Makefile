# shotkey - Suckless hot key daemon
# See LICENSE file for copyright and license details.

include config.mk

SRC = shotkey.c
OBJ = ${SRC:.c=.o}

all: options shotkey

options:
	@echo shotkey build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

config.h: config.def.h
	cp -f config.def.h $@

$(OBJ): config.h

shotkey: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

clean:
	rm -f shotkey ${OBJ} config.h

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f shotkey ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/shotkey
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < shotkey.1 > ${DESTDIR}${MANPREFIX}/man1/shotkey.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/shotkey.1

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/shotkey\
		${DESTDIR}${MANPREFIX}/man1/shotkey.1

.PHONY: all options clean dist install uninstall
