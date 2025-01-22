# Sun/Solaris & Linux Makefile
#

#CC=gcc
CC=musl-gcc
#CFLAGS=-O2 -pipe
# for UTF-8, set -DCOLOR=1 and add '-DUTF8=1'
CFLAGS=-DCOLOR=1 -DUTF8=1 -pipe -g -O2 -fpie

OBJS=ansi.o basic.o buffer.o display.o error.o file.o fileio.o line.o main.o \
	random.o region.o search.o spawn.o termio.o window.o word.o \
	fontlock.o
# televideo.o

me: $(OBJS)
	$(CC) -static -pie -o $@ $(OBJS)

$(OBJS): ed.h
fontlock.o: color.h
fontlock_cpp.c: fontlock_cpp.gperf
	gperf -c -D -k1,3 -LANSI-C -Nis_cpp_keyword fontlock_cpp.gperf >fontlock_cpp.c

#temporary, waiting for an autoconfigured Makefile...
VERSION=2.29
dist:
	ln -s . me-$(VERSION); \
	tar -zcvf me-$(VERSION).tar.gz \
		me-$(VERSION)/Changes \
		me-$(VERSION)/README \
		me-$(VERSION)/ansi.c \
		me-$(VERSION)/atari.c \
		me-$(VERSION)/basic.c \
		me-$(VERSION)/buffer.c \
		me-$(VERSION)/color.h \
		me-$(VERSION)/display.c \
		me-$(VERSION)/ed.h \
		me-$(VERSION)/error.c \
		me-$(VERSION)/file.c \
		me-$(VERSION)/fileio.c \
		me-$(VERSION)/fontlock.c \
		me-$(VERSION)/fontlock_cpp.c \
		me-$(VERSION)/fontlock_cpp.gperf \
		me-$(VERSION)/helplib.c \
		me-$(VERSION)/ibmpc.c \
		me-$(VERSION)/line.c \
		me-$(VERSION)/lookup.c \
		me-$(VERSION)/main.c \
		me-$(VERSION)/makefile \
		me-$(VERSION)/makefile.ch \
		me-$(VERSION)/makefile.sun \
		me-$(VERSION)/makefile.ztc \
		me-$(VERSION)/me.doc \
		me-$(VERSION)/me.lnk \
		me-$(VERSION)/random.c \
		me-$(VERSION)/region.c \
		me-$(VERSION)/search.c \
		me-$(VERSION)/spawn.c \
		me-$(VERSION)/tcap.c \
		me-$(VERSION)/televideo.c \
		me-$(VERSION)/termio.c \
		me-$(VERSION)/vt52.c \
		me-$(VERSION)/window.c \
		me-$(VERSION)/word.c
	rm me-$(VERSION)
