## Makefile

IMGS := build

all: $(IMGS)

clean: TARGET =clean
clean: all

build:
	$(MAKE) -C $@ $(TARGET)

.PHONY: all clean $(IMGS)
