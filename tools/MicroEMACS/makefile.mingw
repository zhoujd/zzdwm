# Makefile - makefile input file for MicroEMACS.

# The top-level source directory.

srcdir = .

# Directories containing system- and tty-specific source.

TTY = ./tty/mingw
SYS = ./sys/mingw
VPATH = $(SYS):$(TTY)
INC = -I$(srcdir) -I$(SYS) -I$(TTY)

# Compilers, linkers, and their flags.

CC = gcc
CFLAGS = -std=c99 -D_POSIX_C_SOURCE=200809L -DMINGW -DBACKUP $(INC) -Wall
ifeq ($(DEBUG), yes)
CFLAGS := -g -O0 $(CFLAGS)
else
CFLAGS := -Os $(CFLAGS)
endif

LD = gcc
LDFLAGS = -s

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

TARGET = me.exe

$(TARGET): $(OBJ)
	$(LD) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)

rev.h:
	@$(srcdir)/ver.sh

version.o: rev.h

strip: $(TARGET)
	strip -v $(TARGET)

clean:
	rm -f *.[o] $(TARGET) rev.h
