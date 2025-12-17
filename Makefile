CFLAGS=-std=c99 -Wall -Wextra -Werror -ggdb $(shell pkg-config --cflags libssh)
LDFLAGS=$(shell pkg-config --libs libssh)

all: tetris

tetris: src/tetris.c
	$(CC) src/tetris.c $(CFLAGS) -o tetris $(LDFLAGS)

clean:
	rm -f tetris

.PHONY: all clean
