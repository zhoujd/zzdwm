## Makefile

vi: vi.c bb.h
	$(CC) vi.c -o vi -std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-implicit-fallthrough -Os -s

debug: vi.c bb.h
	$(CC) vi.c -o vi -Wall -Wextra -Wno-unused-parameter -Wno-implicit-fallthrough -std=c99 -g -O0

clean:
	rm -f vi *.o

install:
	cp -f vi /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/vi
