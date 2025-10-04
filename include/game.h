#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>

#define ROWS 6
#define COLS 7

typedef enum { CELL_EMPTY='.', CELL_A='A', CELL_B='B' } Cell;

typedef struct {
    Cell grid[ROWS][COLS];   // row 0 = top, row 5 = bottom
    int  heights[COLS];      // how many pieces currently in each column
} Board;

// Initialize to all empty
void board_init(Board *b);

// Attempt to drop 'piece' into column [1..7]; returns false if column full or out of range.
// On success, writes the row index via out_row (optional, can be NULL).
bool board_drop(Board *b, int col1_based, Cell piece, int *out_row);

// True if last move at (r,c) completed a 4-in-a-row for 'piece'
bool board_is_winning(const Board *b, int r, int c, Cell piece);

// True if every column is full
bool board_is_full(const Board *b);

// Print board to stdout in the exact ASCII layout required
void board_print(const Board *b);

#endif
