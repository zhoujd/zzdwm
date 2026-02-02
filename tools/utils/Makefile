## Makefile

UTILS := \
	blackwalls

all: $(UTILS)

clean: TARGET=clean
clean: all

install: TARGET=install
install: all

uninstall: TARGET=uninstall
uninstall: all

$(UTILS):
	$(MAKE) -C $@ $(TARGET)

.PHONY: all install uninstall clean $(UTILS)
