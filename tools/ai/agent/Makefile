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

release:
	@if [ -n "$$INSIDE_DOCKER" ]; then \
		$(MAKE) clean; \
		$(MAKE) all; \
	elif command -v musl-gcc >/dev/null 2>&1; then \
		$(MAKE) clean; \
		$(MAKE) CC=musl-gcc all; \
	else \
		echo "Building with Docker musl image $${MUSL_IMG:-zhoujd/alpine}"; \
		docker run --rm -i \
			-u "$$(id -u):$$(id -g)" \
			-e INSIDE_DOCKER=1 \
			-v "$(REPO_ROOT):$(REPO_ROOT)" \
			-w "$(BUILD_DIR)" \
			"$${MUSL_IMG:-zhoujd/alpine}" \
			sh -c 'cat /etc/os-release && $(MAKE) -C "$$PWD" all'; \
	fi

all: $(TARGET)

CJSON_SOURCE = cJSON.c
UTF_SOURCE = rune.c

$(TARGET): $(PROGRAM).c $(CJSON_SOURCE) $(VERSION_HEADER)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(TUI_TARGET): $(TUI_SOURCE) $(UTF_SOURCE) $(VERSION_HEADER)
	$(CC) $(CFLAGS) -o $@ $^ $(shell pkg-config --libs ncursesw)

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
