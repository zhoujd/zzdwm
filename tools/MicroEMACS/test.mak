## Test Makefile

TARGET = utf8 regtest tcap reftag

CC = gcc

all: $(TARGET)

utf8: utf8.c
	$(CC) -o $@ -I. -Isys/unix -Itty/ncurses -DTEST $^

regtest: regtest.c regsub.c regexp.c regexp.h
	$(CC) -o $@ -I. $^

tcap: tcap.c
	$(CC) -o $@ $^ -lncursesw

reftag: reftag.c
	$(CC) -o $@ $^

clean:
	rm -f $(TARGET)
