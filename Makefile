# Makefile
CC=gcc
CFLAGS=-std=c11 -Wall -Wextra -Wpedantic -O2 -pthread
INCS=-Iinclude
SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)
BIN=bin/connect4
TESTBIN=bin/tests

.PHONY: all run test clean format

all: $(BIN)

bin:
	mkdir -p bin

$(BIN): bin $(OBJ) app/main.o
	$(CC) $(CFLAGS) $(INCS) $(OBJ) app/main.o -o $(BIN)

src/%.o: src/%.c include/%.h
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@

app/main.o: app/main.c include/board.h include/game.h
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@

run: $(BIN)
	./$(BIN)

# simple, header-only tests compiled as a separate main
test: $(OBJ) tests/test_main.o
	$(CC) $(CFLAGS) $(INCS) $(OBJ) tests/test_main.o -o $(TESTBIN)
	./$(TESTBIN)

tests/test_main.o: tests/test_main.c include/board.h
	$(CC) $(CFLAGS) $(INCS) -c $< -o $@

format:
	clang-format -i app/*.c src/*.c include/*.h tests/*.c || true

clean:
	rm -rf bin src/*.o app/*.o tests/*.o
