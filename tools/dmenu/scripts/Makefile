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

.PHONY: all
all:
	@echo "Usage:"
	@echo "sudo make install"
	@echo "sudo make uninstall"

.PHONY: install $(addprefix install_,$(TOOLS))
install: $(addprefix install_,$(TOOLS))

.PHONY: uninstall $(addprefix uninstall_,$(TOOLS))
uninstall: $(addprefix uninstall_,$(TOOLS))

define TOOL_template
install_$(1): $(1)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $(1) $(DESTDIR)$(PREFIX)/bin/$(1)
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(1)

uninstall_$(1):
	rm -f $(DESTDIR)$(PREFIX)/bin/$(1)
endef

$(foreach tool,$(TOOLS),$(eval $(call TOOL_template,$(tool))))
