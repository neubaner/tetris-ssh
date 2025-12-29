CFLAGS=-std=c99 -Wall -Wextra -Werror -pedantic -ggdb -O3 -flto=auto -fuse-linker-plugin $(shell pkg-config --cflags libssh)
LDFLAGS=$(shell pkg-config --libs libssh)

all: tetris

obj:
	mkdir -p $@

obj/tetris.o: obj src/tetris.c src/input.h
	+$(CC) $(CFLAGS) -c src/tetris.c -o obj/tetris.o

tetris: obj/tetris.o
	+$(CC) $(CFLAGS) obj/tetris.o -o tetris $(LDFLAGS)

clean:
	rm -rf tetris obj

.PHONY: all clean
