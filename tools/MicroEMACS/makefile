# Sun/Solaris & Linux Makefile
#

CC=gcc
#CC=musl-gcc
#CFLAGS=-O2 -pipe
# for UTF-8, set -DCOLOR=1 and add '-DUTF8=1'
CFLAGS=-DCOLOR=1 -DUTF8=1 -pipe -g -O2 -fpie

OBJS=ansi.o basic.o buffer.o display.o error.o file.o fileio.o line.o main.o \
	random.o region.o search.o spawn.o termio.o window.o word.o \
	fontlock.o

TARGET=em

em: $(OBJS)
	$(CC) -static -pie -o $@ $(OBJS)

$(OBJS): ed.h
fontlock.o: color.h
fontlock_cpp.c: fontlock_cpp.gperf
	gperf -c -D -k1,3 -LANSI-C -Nis_cpp_keyword fontlock_cpp.gperf >fontlock_cpp.c

clean:
	rm -f $(TARGET) *.o
