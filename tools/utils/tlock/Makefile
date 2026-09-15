# ==============================================================================
# Makefile for building slock-lite (Static User-Space Terminal Locker)
# ==============================================================================

# Compiler configurations
CC       = gcc
CFLAGS   = -O2 -Wall -Wextra
LDFLAGS  = -static

# Target binary name and source files
TARGET   = tlock
SRC      = tlock.c
OBJ      = $(SRC:.c=.o)

# Default rule: Compile and link the static binary
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(OBJ) $(TARGET)

# Define phony targets to avoid conflicts with files named 'all' or 'clean'
.PHONY: all clean
