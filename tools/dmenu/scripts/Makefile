## Makefile

PREFIX = /usr/local
SCRIPTS = \
	dmenu_app \
	dmenu_drun \
	dmenu_exit \
	dmenu_man \
	dmenu_menu \
	dmenu_ssh \
	dmenu_win

all: install

install: $(SCRIPTS)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	chmod 755 $^
	cp -f $^ $(DESTDIR)$(PREFIX)/bin

.PHONY: all install
