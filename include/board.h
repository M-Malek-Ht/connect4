#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>

/*
 * Basic board settings for Connect 4.
 */
#define ROWS 6
#define COLS 7

/*
 * Cell state on the board.
 * Values match what is printed.
 */
typedef enum {
    CELL_EMPTY = '.',
    CELL_A     = 'A',
    CELL_B     = 'B'
} Cell;

/*
 * Board:
 *  - grid  : pieces on the board
 *  - heights[c] : how many pieces are in column c (0..ROWS)
 */
typedef struct {
    Cell grid[ROWS][COLS];
    int  heights[COLS];
} Board;

/*
 * Set board to empty (no pieces, all heights = 0).
 */
void board_init(Board *b);

/*
 * Drop a piece in column col1_based (1..COLS).
 * On success:
 *   - updates the board
 *   - if out_row != NULL, stores the row index there
 * Returns true on success, false if column is invalid or full.
 */
bool board_drop(Board *b, int col1_based, Cell piece, int *out_row);

/*
 * Check if piece at (r, c) makes a 4-in-a-row.
 * Returns true if this is a winning move.
 */
bool board_is_winning(const Board *b, int r, int c, Cell piece);

/*
 * Return true if no more moves can be played (all columns full).
 */
bool board_is_full(const Board *b);

/*
 * Print the board to stdout in a simple text grid.
 */
void board_print(const Board *b);

#endif /* BOARD_H */

