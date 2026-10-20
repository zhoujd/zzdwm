# Makefile
CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
TARGET = test
OBJS = cJSON.o test.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

test.o: test.c
	$(CC) $(CFLAGS) -c test.c

cJSON.o: cJSON.c
	$(CC) $(CFLAGS) -c cJSON.c

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
