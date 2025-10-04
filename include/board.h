#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>

#define ROWS 6
#define COLS 7

typedef enum { CELL_EMPTY='.', CELL_A='A', CELL_B='B' } Cell;

typedef struct {
    Cell grid[ROWS][COLS]; 
    int  heights[COLS];     
} Board;

void board_init(Board *b);

bool board_drop(Board *b, int col1_based, Cell piece, int *out_row);

bool board_is_winning(const Board *b, int r, int c, Cell piece);

bool board_is_full(const Board *b);

void board_print(const Board *b);

#endif

