## Test Makefile

ROOT = ..
TARGET = utf8 regtest tcap reftag syntax-test
CFLAGS := -Os

ifeq ($(INSIDE_DOCKER), yes)
LDFLAGS := -static -s
else
LDFLAGS := -s
endif

CC = gcc

all: $(TARGET)

debug: CFLAGS = -Wall -Wextra -g -O0 -DDEBUG -fsanitize=address
debug: LDFLAGS += -fsanitize=address
debug: $(TARGET)

utf8: $(ROOT)/utf8.c utf8.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ -I$(ROOT) -I$(ROOT)/sys/unix \
		-I$(ROOT)/tty/ncurses -DTEST $^

regtest: regtest.c $(ROOT)/regsub.c $(ROOT)/regexp.c $(ROOT)/regexp.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ -I$(ROOT) $^

tcap: tcap.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lncursesw

reftag: reftag.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

syntax-test: syntax.c $(ROOT)/utf8.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ -I$(ROOT) -I$(ROOT)/sys/unix \
		-I$(ROOT)/tty/ncurses -DCOLOR -DTEST $^

clean:
	rm -f $(TARGET) *.[o]
