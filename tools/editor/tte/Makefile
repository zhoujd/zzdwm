## Makefile

tte: tte.c
	$(CC) tte.c -o tte -std=c99 -static -s

debug: tte.c
	$(CC) tte.c -o tte -Wall -Wextra -pedantic -std=c99 -g -O0

clean:
	rm -f tte *.o

install:
	cp -f tte /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/tte
