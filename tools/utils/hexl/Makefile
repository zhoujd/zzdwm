# vim:ts=8

CC	?= cc
CFLAGS	?= -O2
CFLAGS	+= -Wall

PREFIX	?= /usr/local
BINDIR	?= $(PREFIX)/bin

INSTALL_PROGRAM ?= install

# includes and libs
INCLUDES =
LDFLAGS = -static -s

PROG	= hexl
OBJS	= hexl.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(OBJS): *.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL_PROGRAM) $(PROG) $(DESTDIR)$(BINDIR)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(PROG)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all install uninstall clean
