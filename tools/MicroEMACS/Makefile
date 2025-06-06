# makefile - makefile input file for MicroEMACS.

# The top-level source directory.

srcdir		= .

# Directories containing system- and tty-specific source.

SYS		= ./sys/unix
TTY		= ./tty/ncurses
VPATH		= $(SYS):$(TTY)
INC		= -I$(srcdir) -I$(SYS) -I$(TTY)

# Compilers, linkers, and their flags.

CC		= gcc
CFLAGS_DEBUG	= -g -DDEBUG=1
CFLAGS_OPTIMIZE	= -Os
CFLAGS		= $(CFLAGS_OPTIMIZE) -DLINUX $(INC) -Wall -DPREFIX=/usr/local

LD		= gcc
LDFLAGS		= -L/usr/lib64/termcap
LIBS		= -lncursesw

# Objects comprising MicroEMACS.

OBJ :=	 \
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
	utf8.o

# How to compile a module.

.c.o:
	$(CC) -c $(CFLAGS) $<

# How to link MicroEMACS.

me:	$(OBJ)
	$(LD) -o me $(LDFLAGS) $(OBJ) $(LIBS)

install: me
	strip me
	cp -p me /usr/local/bin

clean:
	rm -f *.[o] me

