# Makefile for Connect4: builds main game, optional tests, and utility targets.

# Compiler and flags
CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -O2 -I. -Iinclude -pthread
LDFLAGS := -pthread

# Output binaries
BIN_DIR := bin
BIN     := $(BIN_DIR)/connect4
TESTBIN := $(BIN_DIR)/tests

# Core source files and objects
SRC := app/main.c src/board.c src/game.c
OBJ := $(SRC:.c=.o)

# Test sources and objects (if present)
TEST_SRC  := $(wildcard test_main.c) $(shell [ -d tests ] && find tests -maxdepth 2 -type f -name '*.c' || true)
TEST_OBJS := $(TEST_SRC:.c=.o)
NONMAIN_OBJS := $(filter-out app/main.o,$(OBJ))

.PHONY: all run test clean list debug sanitize

# Default build: game executable
all: $(BIN)

# Ensure binary output directory exists
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# Link main game binary
$(BIN): $(OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJ) -o $@

# Compile any .c into matching .o (create subdirs as needed)
%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Run the main game
run: $(BIN)
	./$(BIN)

# Build and run tests if any test sources are found
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

# Show detected sources, objects, and test inputs
list:
	@echo "Sources:";            printf "  %s\n" $(SRC); \
	 echo "Objects:";            printf "  %s\n" $(OBJ); \
	 echo "Test sources:";       printf "  %s\n" $(TEST_SRC); \
	 echo "Non-main objs:";      printf "  %s\n" $(NONMAIN_OBJS)

# Remove binaries and intermediate build files
clean:
	rm -rf $(BIN_DIR) \
	       app/*.o app/*.d \
	       src/*.o  src/*.d  \
	       tests/*.o tests/*.d \
	       test_main.o *.d

# Rebuild with debug info and no optimization (-O0 -g)
debug:
	@echo "[debug] Cleaning and rebuilding with -O0 -g ..."
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -O0 -g -I. -Iinclude'

# Rebuild with sanitizers (AddressSanitizer + UBSan) enabled
sanitize:
	@echo "[sanitize] Cleaning and rebuilding with ASan+UBSan ..."
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c11 -Wall -Wextra -Wpedantic -O1 -g -I. -Iinclude -fsanitize=address,undefined' \
	        LDFLAGS='-fsanitize=address,undefined'

