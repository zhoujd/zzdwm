PROGRAM = ai
TARGET = ai
DEST ?= /usr/local/bin

CFLAGS ?= -std=c11 -Os -pipe -Wall -Wextra -Werror
CFLAGS += -D_FORTIFY_SOURCE=2
LDFLAGS ?= -static -s
LDFLAGS += -Wl,-z,relro,-z,now,-z,noexecstack

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
			-v "$$(git rev-parse --show-toplevel):$$(git rev-parse --show-toplevel)" \
			-w "$$(pwd)" \
			"$${MUSL_IMG:-zhoujd/alpine}" \
			sh -c 'cat /etc/os-release && $(MAKE) release'; \
	fi

all: $(TARGET)

$(TARGET): $(PROGRAM).c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

install: $(TARGET)
	mkdir -p $(DEST)
	install -m 0755 $(TARGET) $(DEST)/$(PROGRAM)

clean:
	rm -f $(TARGET)

test: $(TARGET)
	python3 test/test_agent.py

.PHONY: all clean release test install
