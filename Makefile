CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu17 -D_GNU_SOURCE
RELEASE_FLAGS = -O2
DEBUG_FLAGS = -g
OBJS = build/buffer.o build/parser.o build/main.o build/bytecode.o build/ast.o build/optimizer.o
BIN = build/parser_example

ifeq ($(RELEASE), ON)
	CFLAGS += $(RELEASE_FLAGS)
else
	CFLAGS += $(DEBUG_FLAGS)
endif

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(BIN)

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rv $(OBJS) $(BIN)
