# Makefile - makefile input file for the ai agent.

PROGRAM = ai
TARGET = ai
DEST ?= /usr/local/bin
VERSION_HEADER = version.h

# Objects comprising the agent. One binary holds the backend, the
# ncurses TUI, and the editor, so every toolchain links ncursesw.

OBJ := \
	ai.o \
	main.o \
	tui.o \
	cJSON.o \
	rune.o \
	editor.o \
	tool.o \
	landlock.o \
	config.o

HEADERS = agent.h editor.h tool.h landlock.h config.h

# Compiler, linker, and their flags. DEBUG keeps the static link and
# the hardening flags but swaps -Os for -g -O0 and drops the link-time
# strip, so gdb sees symbols on the same shape of binary the publish
# build ships.

DEBUG ?= no
CFLAGS ?= -std=c11 -pipe -Wall -Wextra -Werror
CFLAGS += -D_FORTIFY_SOURCE=2
LDFLAGS ?= -static -s
LDFLAGS += -Wl,-z,relro,-z,now,-z,noexecstack
LDLIBS += -lpthread
CFLAGS += $(shell pkg-config --cflags ncursesw)
LDLIBS += $(shell pkg-config --libs ncursesw)

ifeq ($(DEBUG), yes)
CFLAGS := -g -O0 $(CFLAGS)
LDFLAGS := $(filter-out -s,$(LDFLAGS))
else
CFLAGS := -Os $(CFLAGS)
endif

BUILD_FLAGS = $(CC) $(CFLAGS) $(LDFLAGS) $(LDLIBS)
BUILD_FLAGS_FILE = .build-flags

# The \# escape keeps make from treating the include as a comment.
# Headers alone do not guarantee the static archives exist, so the
# static probe links a full program.
NCURSES_PROBE = printf '%s\n' '\#include <ncurses.h>' 'int main(void) { return 0; }' | \
                  $(1) -x c -o /dev/null - >/dev/null 2>&1
STATIC_LINK_PROBE = printf '%s\n' '\#include <ncurses.h>' 'int main(void) { return 0; }' | \
                      $(1) -x c $(LDFLAGS) -o /dev/null - $(LDLIBS) >/dev/null 2>&1

# How to build a release. The publish build runs inside Docker with
# INSIDE_DOCKER set; on a host it links statically when cc can, and
# otherwise falls back to a dynamically linked binary.

release:
	@if [ -n "$$INSIDE_DOCKER" ]; then \
		$(MAKE) clean; \
		$(MAKE) all; \
	elif $(call STATIC_LINK_PROBE,$(CC)); then \
		$(MAKE) clean; \
		$(MAKE) all; \
	elif $(call NCURSES_PROBE,$(CC)); then \
		echo "No toolchain links ncursesw statically; building dynamically linked ai" >&2; \
		$(MAKE) clean; \
		$(MAKE) all LDFLAGS='-s -Wl,-z,relro,-z,now,-z,noexecstack'; \
	else \
		echo "error: ai needs ncurses headers; install libncursesw-dev or run build.sh -p" >&2; \
		exit 1; \
	fi

all: $(TARGET)

# Record the flags so a toggle such as DEBUG=yes rebuilds every object
# instead of mixing old and new compilations.

$(BUILD_FLAGS_FILE): FORCE
	@printf '%s\n' '$(BUILD_FLAGS)' > $@.tmp
	@if cmp -s $@.tmp $@; then \
	  rm -f $@.tmp; \
	else \
	  mv -f $@.tmp $@; \
	fi

# How to compile a module.

.c.o:
	$(CC) -c $(CFLAGS) $<

# How to link the agent.

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

# Any header or version change rebuilds every object, matching the
# single-shot compile this file replaced; version.sh rewrites the
# header only when the stamp changes, so this stays incremental.

$(OBJ): $(BUILD_FLAGS_FILE) $(VERSION_HEADER) $(HEADERS)

$(VERSION_HEADER): FORCE
	@./version.sh $@

install: $(TARGET)
	mkdir -p $(DEST)
	install -m 0755 $(TARGET) $(DEST)/$(PROGRAM)

clean:
	rm -f *.o $(TARGET) $(VERSION_HEADER) $(BUILD_FLAGS_FILE) \
	      $(BUILD_FLAGS_FILE).tmp

test: $(TARGET)
	python3 test/test_agent.py

# Kept out of "test": these cases replay full sessions and compile the
# patched files with cc, which not every CI runner can do.
test-ai: $(TARGET)
	python3 test/test_ai.py

FORCE:

.PHONY: all clean release test test-ai install FORCE
