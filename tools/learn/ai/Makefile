# Compiler settings
CC      = gcc
CFLAGS  = -Wall -Wextra -O2

# Linked libraries: Includes wide-character Ncurses and multithreading support
LIBS    = -lncursesw -lpthread

# Build target and object configuration
TARGET  = agent_tui
SRCS    = agent_tui.c
OBJS    = $(SRCS:.c=.o)

# Default rule: Compiles the entire project
all: check_deps $(TARGET)

# Dependency pre-check: Verifies that the ncursesw development package is installed on the system
check_deps:
	@pkg-config --exists ncursesw || \
	(echo "Error: ncursesw development library is missing!" && \
	 echo "Install it via: sudo apt install libncursesw5-dev (Debian/Ubuntu) or sudo dnf install ncurses-devel (Fedora/RHEL)" && \
	 exit 1)

# Link rule to generate the executable binary
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

# Compilation rule to translate C source into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Cleanup rule: Safely removes the compiled binary and intermediary object files
clean:
	rm -f $(TARGET) $(OBJS)

# Execution rule: Automatically triggers a fresh build and instantly launches the TUI environment
run: all
	./$(TARGET)

# Explicitly state that these are phony targets to prevent naming conflicts with physical files
.PHONY: all clean run check_deps
