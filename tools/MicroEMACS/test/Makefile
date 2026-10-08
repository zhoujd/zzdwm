## Test Makefile

ROOT = ..
TARGET = utf8 regtest tcap reftag syntax-test display-test cscope-test
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

display-test: display.c $(ROOT)/utf8.c
	$(CC) $(CFLAGS) -Wno-unused-parameter -ffunction-sections -fdata-sections $(LDFLAGS) \
		-Wl,--gc-sections -o $@ -I$(ROOT) -I$(ROOT)/sys/mingw \
		-I$(ROOT)/tty/mingw -DMINGW -DCOLOR -DTEST $^

cscope-test: cscope.c $(ROOT)/cscope.c $(ROOT)/sys/unix/spawn.c
	$(CC) $(CFLAGS) -Wno-unused-parameter -ffunction-sections -fdata-sections $(LDFLAGS) \
		-Wl,--gc-sections -o $@ -I$(ROOT) -I$(ROOT)/sys/unix \
		-I$(ROOT)/tty/ncurses -DTEST cscope.c $(ROOT)/sys/unix/spawn.c

clean:
	rm -f $(TARGET) *.[o]
