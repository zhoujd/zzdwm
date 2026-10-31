# Makefile - makefile input file for MicroEMACS.

OS = $(shell uname -s)

ifeq ($(OS), Linux)
MF = Make.Linux
TARGET = me
else
MF = Make.Mingw
TARGET = me.exe
endif

all:
	$(MAKE) -f $(MF) $(TARGET)

min:
	$(MAKE) -f $(MF) COLOR=no LISP=no AI=no MOUSE=no MINIMAL=yes $(TARGET)

clean: TARGET=clean
clean: all

install: TARGET=install
install: all

uninstall: TARGET=uninstall
uninstall: all

strip: TARGET=strip
strip: all

help:
	@echo "make MF=Make.Linux"
	@echo "make MF=Make.Mingw"
	@echo "make min"
	@echo "make -C test"

.PHONY: all min clean install uninstall strip help
