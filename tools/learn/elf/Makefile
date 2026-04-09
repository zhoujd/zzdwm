## Makefile

TARGET = elf_parser

CC := gcc
CFLAGS := -O0 -g
LDFLAGS := -lelf

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $< $(LDFLAGS)

clean:
	rm -f $(TARGET) *.o
