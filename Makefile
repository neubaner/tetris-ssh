all: tetris

tetris: src/tetris.c
	$(CC) src/tetris.c -std=c99 -Wall -Wextra $(pkg-config --cflags libssh) -o tetris $(pkg-config --libs libssh)

clean:
	rm -f tetris

.PHONY: all clean
