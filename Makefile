CFLAGS=-std=c11 -Wall -Wextra -Werror -pedantic -ggdb -O3 -MMD -MP $(shell pkg-config --cflags libssh)
LDFLAGS=$(shell pkg-config --libs libssh) -lpthread
TEST_FLAGS=-DTEST_COLOR_OUTPUT

SOURCES=$(wildcard src/*.c)
OBJS=$(SOURCES:src/%.c=obj/%.o)
DEPS=$(OBJS:%.o=%.d)

TEST_SOURCES=$(wildcard tests/*.c)
TEST_BINS=$(TEST_SOURCES:tests/%.c=obj/%)
TEST_DEPS=$(TEST_BINS:%=%.d)

all: tetris tests

obj/%.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

obj/%.o: tests/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

tetris: $(OBJS)
	$(CC) $(CFLAGS) $^ -o tetris $(LDFLAGS)

obj/%_test: tests/%_test.c $(filter-out obj/tetris.o,$(OBJS))
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(TEST_FLAGS) $^ -o $@ $(LDFLAGS)
	./$@

tests: $(TEST_BINS)

clean:
	rm -rf tetris obj

.PHONY: all clean tests

-include $(DEPS) $(TEST_DEPS)
