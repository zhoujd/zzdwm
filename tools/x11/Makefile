## Makefile

APPS := \
	blackwalls

all: $(APPS)

clean: TARGET=clean
clean: all

install: TARGET=install
install: all

uninstall: TARGET=uninstall
uninstall: all

$(APPS):
	$(MAKE) -C $@ $(TARGET)

.PHONY: all install uninstall clean $(APPS)
