CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu17 -O2 -g -D_GNU_SOURCE
OBJS = build/buffer.o build/parser.o build/main.o build/bytecode.o
BIN = build/parser_example

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(BIN)

build/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rv $(OBJS) $(BIN)
