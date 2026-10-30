## Test Makefile

ROOT = ..
TARGET = utf8 regtest tcap reftag tag-test symbol-test syntax-test \
	display-test mouse-test cscope-test lisp-test ai-test path-test
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

tag-test: tag.c $(ROOT)/regexp.c $(ROOT)/regexp.h $(ROOT)/utf8.c
	$(CC) $(CFLAGS) -Wno-unused-parameter -ffunction-sections -fdata-sections \
		$(LDFLAGS) -Wl,--gc-sections -o $@ -DCOLOR -I$(ROOT) \
		-I$(ROOT)/sys/unix -I$(ROOT)/tty/ncurses $^

symbol-test: symbol.c $(ROOT)/def.h $(ROOT)/symbol.c
	$(CC) $(CFLAGS) -Wno-unused-parameter -ffunction-sections -fdata-sections \
		$(LDFLAGS) -Wl,--gc-sections -o $@ -DCOLOR -I$(ROOT) \
		-I$(ROOT)/sys/unix -I$(ROOT)/tty/ncurses symbol.c $(ROOT)/def.h

syntax-test: syntax.c $(ROOT)/utf8.c $(ROOT)/syntax.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ -I$(ROOT) -I$(ROOT)/sys/unix \
		-I$(ROOT)/tty/ncurses -DCOLOR -DTEST syntax.c $(ROOT)/utf8.c

display-test: display.c $(ROOT)/path.c $(ROOT)/utf8.c
	$(CC) $(CFLAGS) -Wno-unused-parameter -ffunction-sections -fdata-sections $(LDFLAGS) \
		-Wl,--gc-sections -o $@ -I$(ROOT) -I$(ROOT)/sys/mingw \
		-I$(ROOT)/tty/mingw -DMINGW -DCOLOR -DTEST $^

mouse-test: mouse.c $(ROOT)/path.c $(ROOT)/utf8.c
	$(CC) $(CFLAGS) -Wno-unused-parameter -ffunction-sections -fdata-sections $(LDFLAGS) \
		-Wl,--gc-sections -o $@ -I$(ROOT) -I$(ROOT)/sys/mingw \
		-I$(ROOT)/tty/mingw -DMINGW -DCOLOR -DTEST $^

cscope-test: cscope.c $(ROOT)/cscope.c $(ROOT)/sys/unix/spawn.c
	$(CC) $(CFLAGS) -Wno-unused-parameter -ffunction-sections -fdata-sections $(LDFLAGS) \
		-Wl,--gc-sections -o $@ -I$(ROOT) -I$(ROOT)/sys/unix \
		-I$(ROOT)/tty/ncurses -DTEST cscope.c $(ROOT)/sys/unix/spawn.c

lisp-test: lisp.c $(ROOT)/lisp.c $(ROOT)/tinylisp.c $(ROOT)/tinylisp.h \
		$(ROOT)/path.c $(ROOT)/utf8.c
	$(CC) $(CFLAGS) -Wno-unused-parameter -ffunction-sections -fdata-sections \
		-fno-asynchronous-unwind-tables $(LDFLAGS) -Wl,--gc-sections \
		-o $@ -DCOLOR -DLISP -DAI -I$(ROOT) -I$(ROOT)/sys/unix \
		-I$(ROOT)/tty/ncurses \
		$< $(ROOT)/tinylisp.c $(ROOT)/path.c $(ROOT)/utf8.c

ai-test: ai.c $(ROOT)/ai.c $(ROOT)/def.h
	$(CC) $(CFLAGS) -Wno-unused-parameter -ffunction-sections -fdata-sections \
		$(LDFLAGS) -Wl,--gc-sections -o $@ -DLISP -DAI \
		-I$(ROOT) -I$(ROOT)/sys/unix -I$(ROOT)/tty/ncurses $<

path-test: path.c $(ROOT)/path.c $(ROOT)/def.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ -I$(ROOT) \
		-I$(ROOT)/sys/unix -I$(ROOT)/tty/ncurses $<

path-mingw-test: path.c $(ROOT)/path.c $(ROOT)/def.h $(ROOT)/sys/mingw/fileio.c
	$(CC) $(CFLAGS) -D_GNU_SOURCE -DMINGW -ffunction-sections \
		-fdata-sections $(LDFLAGS) -Wl,--gc-sections -o $@ \
		-I$(ROOT) -I$(ROOT)/sys/mingw -I$(ROOT)/tty/mingw $<

clean:
	rm -f $(TARGET) *.[o]
