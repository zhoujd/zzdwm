CC = cc
CFLAGS = -Wall -O2
LDFLAGS = -static -s

all: ram
.c.o:
	$(CC) -c $(CFLAGS) $<
ram: ram.o
	$(CC) $(LDFLAGS) -o $@ $^
clean:
	rm -f *.o ram
