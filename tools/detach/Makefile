# vim:ts=8

PROG = detach
VER  = "0.2.0"

CC	?= cc
CFLAGS	?= -O2
CFLAGS	+= -Wall -DPACKAGE=\"$(PROG)\" -DVERSION=\"$(VER)\"

PREFIX	?= /usr/local
BINDIR	?= $(PREFIX)/bin

INSTALL_PROGRAM ?= install -s

INCLUDES ?= 
LDFLAGS	+=

OBJS	= detach.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(OBJS): *.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL_PROGRAM) $(PROG) $(DESTDIR)$(BINDIR)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all install clean
