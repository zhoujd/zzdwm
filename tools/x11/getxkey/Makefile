## Makefile

# Define compiler and compiler flags
CC = gcc
CFLAGS = -Wall -O2
LIBS = -L/usr/X11R6/lib -lX11

# Define target executable and source files
TARGET = getxkey
SRC = getxkey.c

# Default target (executed when running 'make')
all: $(TARGET)

# Compilation rule
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LIBS) -o $(TARGET)

# Clean build artifacts (executed when running 'make clean')
clean:
	rm -f $(TARGET)

.PHONY: all clean
