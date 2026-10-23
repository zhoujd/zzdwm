PROGRAM = ai
TARGET = ai
TUI_SOURCE = ai_tui.c
TUI_TARGET = ai-tui
DEST ?= /usr/local/bin
VERSION_HEADER = version.h
REPO_ROOT := $(shell git rev-parse --show-toplevel)
BUILD_DIR := $(REPO_ROOT)/tools/ai/agent

CFLAGS ?= -std=c11 -Os -pipe -Wall -Wextra -Werror
CFLAGS += -D_FORTIFY_SOURCE=2
LDFLAGS ?= -static -s
LDFLAGS += -Wl,-z,relro,-z,now,-z,noexecstack
LDLIBS += -lpthread
TUI_CFLAGS += $(shell pkg-config --cflags ncursesw)
TUI_LDFLAGS ?= -static -s
TUI_LDFLAGS += -Wl,-z,relro,-z,now,-z,noexecstack
TUI_LDLIBS ?= $(shell pkg-config --libs ncursesw)

# musl-gcc compiles with -nostdinc, so it cannot use the host glibc
# ncurses headers; probe each toolchain instead of assuming it works.
# The \# escape keeps make from treating the include as a comment.
NCURSES_PROBE = printf '%s\n' '\#include <ncurses.h>' 'int main(void) { return 0; }' | $(1) -x c -o /dev/null - >/dev/null 2>&1

release:
	@if [ -n "$$INSIDE_DOCKER" ]; then \
		$(MAKE) clean; \
		$(MAKE) all; \
	elif command -v musl-gcc >/dev/null 2>&1 \
		&& $(call NCURSES_PROBE,musl-gcc); then \
		$(MAKE) clean; \
		$(MAKE) CC=musl-gcc all; \
	elif $(call NCURSES_PROBE,$(CC)); then \
		$(MAKE) clean; \
		if command -v musl-gcc >/dev/null 2>&1; then \
			echo "Building ai with musl-gcc; ai-tui with $(CC) because musl-gcc lacks ncursesw"; \
			$(MAKE) CC=musl-gcc $(TARGET); \
		else \
			$(MAKE) $(TARGET); \
		fi; \
		$(MAKE) $(TUI_TARGET); \
	else \
		$(MAKE) clean; \
		$(MAKE) $(TARGET); \
		echo "note: no toolchain can compile ncurses.h; skipped $(TUI_TARGET)"; \
	fi

all: $(TARGET) $(TUI_TARGET)

CJSON_SOURCE = cJSON.c
UTF_SOURCE = rune.c

$(TARGET): $(PROGRAM).c $(CJSON_SOURCE) $(VERSION_HEADER)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(TUI_TARGET): $(TUI_SOURCE) $(UTF_SOURCE) $(VERSION_HEADER)
	$(CC) $(CFLAGS) $(TUI_CFLAGS) $(TUI_LDFLAGS) -o $@ $^ $(TUI_LDLIBS)

$(VERSION_HEADER): FORCE
	@./version.sh $@

install: $(TARGET)
	mkdir -p $(DEST)
	install -m 0755 $(TARGET) $(DEST)/$(PROGRAM)

clean:
	rm -f $(TARGET) $(TUI_TARGET) $(VERSION_HEADER)

test: $(TARGET)
	python3 test/test_agent.py

.PHONY: all clean release test install FORCE

FORCE:
