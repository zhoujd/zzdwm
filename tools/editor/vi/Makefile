## Makefile

CFLAGS = -std=c99 -D_GNU_SOURCE \
         -Wall -Wextra -Wno-unused-parameter -Wno-implicit-fallthrough

ifeq ($(DEBUG), yes)
CFLAGS := -g -O0 $(CFLAGS)
else
CFLAGS := -Os -ffunction-sections -fdata-sections \
          -fno-asynchronous-unwind-tables $(CFLAGS)
endif

LDFLAGS =
ifeq ($(STATIC), yes)
LDFLAGS += -static -Wl,--gc-sections -Wl,-z,noseparate-code
endif

all: vi

vi: vi.c bb.h
	$(CC) $(CFLAGS) vi.c -o vi $(LDFLAGS)

strip: all
	strip -v vi

clean:
	rm -f vi *.o

install: all
	cp -f vi /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/vi

.PHONY: all strip clean install uninstall
