# Makefile
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g -Wno-unused-parameter
TARGET = ast_example
SOURCES = main.cpp

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $@ $^

debug: CXXFLAGS = -std=c++17 -Wall -Wextra -g -O0 -DDEBUG
debug: $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all debug run clean
