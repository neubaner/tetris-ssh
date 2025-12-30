CFLAGS=-std=c11 -Wall -Wextra -Werror -pedantic -ggdb -O3 -flto=auto -fuse-linker-plugin $(shell pkg-config --cflags libssh)
LDFLAGS=$(shell pkg-config --libs libssh) -lpthread

all: tetris tests

obj:
	mkdir -p $@

obj/tetris.o: obj src/tetris.c src/input.h
	+$(CC) $(CFLAGS) -c src/tetris.c -o obj/tetris.o

tetris: obj/tetris.o
	+$(CC) $(CFLAGS) obj/tetris.o -o tetris $(LDFLAGS)

obj/ring_buf_test: tests/ring_buf_test.c tests/test_lib.h src/ring_buf.h
	+$(CC) $(CFLAGS) tests/ring_buf_test.c -DTEST_COLOR_OUTPUT -o $@ $(LDFLAGS)
	./$@

obj/input_test: tests/input_test.c tests/test_lib.h src/input.h src/ring_buf.h
	+$(CC) $(CFLAGS) tests/input_test.c -DTEST_COLOR_OUTPUT -o $@ $(LDFLAGS)
	./$@

tests: obj/ring_buf_test obj/input_test

clean:
	rm -rf tetris obj

.PHONY: all clean tests
