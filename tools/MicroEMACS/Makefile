# Makefile - makefile input file for MicroEMACS.

# The top-level source directory.

srcdir = .

# Directories containing system- and tty-specific source.

SYS = ./sys/unix
TTY = ./tty/ncurses
VPATH = $(SYS):$(TTY)
INC = -I$(srcdir) -I$(SYS) -I$(TTY)

# Compilers, linkers, and their flags.

CC = gcc
DEBUG = yes
CFLAGS = -DLINUX -DBACKUP $(INC) -Wall
ifeq ($(DEBUG), yes)
CFLAGS := -g -O0 -DDEBUG=1 $(CFLAGS)
else
CFLAGS := -Os -s $(CFLAGS)
endif

LD = gcc
LDFLAGS = -L/usr/lib64/termcap
LIBS = -lncursesw

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
	cp -p $(TARGET) $(DEST)

uninstall:
	rm -f $(DEST)

clean:
	rm -f *.[o] $(TARGET) rev.h
