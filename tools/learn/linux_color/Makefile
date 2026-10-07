# Compiler and Target Options
CC       := gcc
CFLAGS   := -Wall -Wextra -Werror -O2
LIBS     := -lncurses
TARGET   := code_renderer

# Directory Layout
SRC_DIR  := src
OBJ_DIR  := obj
BIN_DIR  := bin

# Source File Discovery
# Finds all .c files in the src/ directory
SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# Main Build Rules
.PHONY: all clean run

all: $(BIN_DIR)/$(TARGET)

# Link the executable
$(BIN_DIR)/$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# Compile source files into object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create output directories if they don't exist
$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

# Helper Commands
run: all
	./$(BIN_DIR)/$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
