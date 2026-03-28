# Makefile - makefile input file for MicroEMACS.

OS = $(shell uname -s)

ifeq ($(OS), Linux)
MF = Make.Linux
else
MF = Make.Mingw
endif

all:
	$(MAKE) -f $(MF) $(TARGET)

clean: TARGET=clean
clean: all

install: TARGET=install
install: all

uninstall: TARGET=uninstall
uninstall: all

strip: TARGET=strip
strip: all

.PHONY: all clean install uninstall strip
