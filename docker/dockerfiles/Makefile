## Makefile

IMGS := build

all: $(IMGS)

clean: TARGET =clean
clean: all

$(IMGS):
	$(MAKE) -C $@ $(TARGET)

.PHONY: all clean $(IMGS)
