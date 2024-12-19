# vim:ts=8

CC	?= cc
CFLAGS	?= -O2
CFLAGS	+= -Wall

PREFIX	?= /usr/local
BINDIR	?= $(PREFIX)/bin

INSTALL_PROGRAM ?= install

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# includes and libs
INCLUDES = -I${X11INC}
LDFLAGS = -L${X11LIB} -lX11 -lXcomposite -lXrender -lXext -lXdamage -lXfixes -lstdc++ -lm

PROG	= xcompmgr
OBJS	= xcompmgr-simple.o

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(OBJS): *.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL_PROGRAM) $(PROG) $(DESTDIR)$(BINDIR)

clean:
	rm -f $(PROG) $(OBJS)

.PHONY: all install clean
