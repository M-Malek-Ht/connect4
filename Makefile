# ===== Connect4 — Explicit Makefile for this tree =====
# Sources:
#   app/main.c
#   src/board.c
#   src/game.c
# Headers in: include/  (board.h, game.h)
# Tests (optional): tests/test_main.c or test_main.c

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -O2 -I. -Iinclude
LDFLAGS :=

BIN_DIR := bin
BIN     := $(BIN_DIR)/connect4
TESTBIN := $(BIN_DIR)/tests

# ---- explicit sources/objects for your tree ----
SRC := app/main.c src/board.c src/game.c
OBJ := $(SRC:.c=.o)

# Test sources: either test_main.c at root or any *.c under tests/
TEST_SRC := $(wildcard test_main.c) $(shell [ -d tests ] && find tests -maxdepth 2 -type f -name '*.c' || true)
TEST_OBJS := $(TEST_SRC:.c=.o)
NONMAIN_OBJS := $(filter-out app/main.o,$(OBJ))

.PHONY: all run test clean list debug sanitize

all: $(BIN)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# Link the main game
$(BIN): $(OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $@

# Compile rule (creates subdirs as needed)
%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

# --------- Tests (optional) ---------
test: | $(BIN_DIR)
	@if [ -n "$(TEST_SRC)" ]; then \
	  echo "Building tests from: $(TEST_SRC)"; \
	  for f in $(TEST_SRC); do \
	    d="$${f%/*}"; [ "$$d" = "$$f" ] || mkdir -p "$$d"; \
	    $(CC) $(CFLAGS) -c $$f -o $${f%.c}.o; \
	  done; \
	  $(CC) $(CFLAGS) $(LDFLAGS) $(NONMAIN_OBJS) $(TEST_OBJS) -o $(TESTBIN); \
	  ./$(TESTBIN); \
	else \
	  echo "No tests found (test_main.c or tests/*.c)."; \
	fi

# Show what this Makefile will use
list:
	@echo "Sources:"; printf "  %s\n" $(SRC); \
	 echo "Objects:"; printf "  %s\n" $(OBJ); \
	 echo "Test sources:"; printf "  %s\n" $(TEST_SRC); \
	 echo "Non-main objs (for tests):"; printf "  %s\n" $(NONMAIN_OBJS)

clean:
	rm -rf $(BIN_DIR) \
	       app/*.o app/*.d \
	       src/*.o  src/*.d  \
	       tests/*.o tests/*.d \
	       test_main.o *.d

debug:
	@echo "[debug] Cleaning and rebuilding with -O0 -g ..."
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -O0 -g -I. -Iinclude'

sanitize:
	@echo "[sanitize] Cleaning and rebuilding with ASan+UBSan ..."
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -O1 -g -I. -Iinclude -fsanitize=address,undefined' \
	        LDFLAGS='-fsanitize=address,undefined'

