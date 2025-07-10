## Makefile

PREFIX := /usr/local
TOOLS := \
	dmenu_app \
	dmenu_drun \
	dmenu_exit \
	dmenu_man \
	dmenu_menu \
	dmenu_ssh \
	dmenu_win

all:
	@echo "Usage:"
	@echo "sudo make install"
	@echo "sudo make uninstall"

install: $(addprefix install_,$(TOOLS))
$(addprefix install_,$(TOOLS)): install_%: %
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $< $(DESTDIR)$(PREFIX)/bin/$<
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$<

uninstall: $(addprefix uninstall_,$(TOOLS))
$(addprefix uninstall_,$(TOOLS)):
	rm -f $(DESTDIR)$(PREFIX)/bin/$(patsubst uninstall_%,%,$@)

.PHONY: all install $(addprefix install_,$(TOOLS)) uninstall $(addprefix uninstall_,$(TOOLS))
