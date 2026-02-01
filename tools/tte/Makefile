## Makefile

tte: tte.c
	$(CC) tte.c -o tte -std=c99 -static

debug: tte.c
	$(CC) tte.c -o tte -Wall -Wextra -pedantic -std=c99 -g

clean:
	rm -f tte *.o

install: tte
	sudo cp tte /usr/local/bin/
	sudo chmod +x /usr/local/bin/
