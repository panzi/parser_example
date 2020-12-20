CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu17 -D_GNU_SOURCE
RELEASE_FLAGS = -O2 -DNDEBUG
DEBUG_FLAGS = -g -DDEBUG
SHARED_OBJS = build/buffer.o build/parser.o build/bytecode.o build/ast.o build/optimizer.o
OBJS = build/main.o $(SHARED_OBJS)
BIN = build/parser_example
TEST_BIN = build/tests/test
TEST_OBJS = build/tests/test.o $(patsubst src/%.c,build/%.o,$(wildcard src/tests/test_*.c))

ifeq ($(RELEASE), ON)
	CFLAGS += $(RELEASE_FLAGS)
else
	CFLAGS += $(DEBUG_FLAGS)
endif

.PHONY: all clean test

all: $(BIN)

test: $(TEST_BIN)
	$(TEST_BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(BIN)

$(TEST_BIN): $(TEST_OBJS) $(OBJS)
	$(CC) $(CFLAGS) $(TEST_OBJS) $(SHARED_OBJS) -o $(TEST_BIN)

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

build/tests/%.o: src/tests/%.c
	$(CC) $(CFLAGS) -Isrc -c $< -o $@

clean:
	rm -rv $(OBJS) $(BIN) $(TEST_OBJS) $(TEST_BINS)
