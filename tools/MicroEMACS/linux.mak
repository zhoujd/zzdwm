# Makefile - makefile input file for MicroEMACS.

# The top-level source directory.

srcdir = .

# Directories containing system- and tty-specific source.

ifeq ($(TERMCAP), yes)
TTY = ./tty/termcap
else
TTY = ./tty/ncurses
endif
SYS = ./sys/unix
VPATH = $(SYS):$(TTY)
INC = -I$(srcdir) -I$(SYS) -I$(TTY)

# Compilers, linkers, and their flags.

CC = gcc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -DLINUX -DBACKUP $(INC) \
         -Wall -Wextra -pedantic \
         -Wno-unused-parameter \
         -Wno-implicit-fallthrough
ifeq ($(DEBUG), yes)
CFLAGS := -g -O0 $(CFLAGS)
else
CFLAGS := -Os $(CFLAGS)
endif

LD = gcc
ifeq ($(TERMCAP), yes)
LDFLAGS = -L/usr/lib64/termcap
LIBS = -ltermcap
else
LIBS = -lncursesw
endif

# Objects comprising MicroEMACS.

OBJ := \
	basic.o \
	buffer.o \
	cinfo.o \
	cscope.o \
	display.o \
	echo.o \
	extend.o \
	file.o \
	kbd.o \
	line.o \
	main.o \
	paragraph.o \
	random.o \
	regexp.o \
	region.o \
	regsub.o \
	ring.o \
	search.o \
	spell.o \
	symbol.o \
	version.o \
	window.o \
	word.o \
	fileio.o \
	spawn.o \
	tags.o \
	ttyio.o \
	tty.o \
	ttykbd.o \
	undo.o \
	utf8.o \
	complete.o \
	bufmenu.o

# How to compile a module.

.c.o:
	$(CC) -c $(CFLAGS) $<

# How to link MicroEMACS.

TARGET = me
PREFIX = /usr/local
DEST = $(PREFIX)/bin/$(TARGET)

$(TARGET): $(OBJ)
	$(LD) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)

rev.h:
	@$(srcdir)/ver.sh

version.o: rev.h

strip: $(TARGET)
	strip -v $(TARGET)

install: strip
	cp -f $(TARGET) $(DEST)

uninstall:
	rm -f $(DEST)

clean:
	rm -f *.[o] $(TARGET) rev.h
