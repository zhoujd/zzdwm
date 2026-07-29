## Makefile

IMGS := \
	build \
	tool

all: $(IMGS)

clean: TARGET=clean
clean: all

$(IMGS):
	$(MAKE) -C $@ $(TARGET)

.PHONY: all clean $(IMGS)
