## Test Makefile

TARGET = utf8 regtest tcap reftag
CFLAGS := -Os
LDFLAGS := -static -s

CC = gcc

all: $(TARGET)

utf8: utf8.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ -I. -Isys/unix -Itty/ncurses -DTEST $^

regtest: regtest.c regsub.c regexp.c regexp.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ -I. $^

tcap: tcap.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lncursesw

reftag: reftag.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)
