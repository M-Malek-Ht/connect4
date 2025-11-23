#include "board.h"
#include <stdio.h>

/* ANSI color codes for colored pieces in the terminal. */
#define ANSI_RESET "\x1b[0m"
#define ANSI_A     "\x1b[36m"  /* cyan  */
#define ANSI_B     "\x1b[35m"  /* magenta */

/*
 * board_init
 * ----------
 * Set all cells to empty and reset column heights to zero.
 */
void board_init(Board *b) {
    for (int r = 0; r < ROWS; ++r) {
        for (int c = 0; c < COLS; ++c) {
            b->grid[r][c] = CELL_EMPTY;
        }
    }

    for (int c = 0; c < COLS; ++c) {
        b->heights[c] = 0;
    }
}

/*
 * board_drop
 * ----------
 * Drop a piece into a 1-based column.
 *
 * Returns:
 *   true  if the piece was placed.
 *   false if the column index is invalid or the column is full.
 *
 * If out_row is non-NULL, it receives the row index where the piece lands.
 */
bool board_drop(Board *b, int col1_based, Cell piece, int *out_row) {
    int c = col1_based - 1;
    if (c < 0 || c >= COLS) {
        return false;
    }
    if (b->heights[c] >= ROWS) {
        return false;
    }

    int r = ROWS - 1 - b->heights[c];

    b->grid[r][c] = piece;
    b->heights[c]++;

    if (out_row) {
        *out_row = r;
    }

    return true;
}

/*
 * ray_count
 * ---------
 * Count matching pieces of type p starting from (r, c) and moving
 * in direction (dr, dc), stopping at the first mismatch or edge.
 * Does not count the starting cell.
 */
static int ray_count(const Board *b, int r, int c, int dr, int dc, Cell p) {
    int cnt = 0;

    for (int i = 1; i < 4; i++) {
        int rr = r + dr * i;
        int cc = c + dc * i;

        if (rr < 0 || rr >= ROWS || cc < 0 || cc >= COLS) {
            break;
        }
        if (b->grid[rr][cc] != p) {
            break;
        }
        cnt++;
    }

    return cnt;
}

/*
 * board_is_winning
 * ----------------
 * Check if the piece p at position (r, c) completes a 4-in-a-row
 * horizontally, vertically, or diagonally.
 */
bool board_is_winning(const Board *b, int r, int c, Cell p) {
    const int D[4][2] = {
        { 0,  1},  /* horizontal */
        { 1,  0},  /* vertical   */
        { 1,  1},  /* diag down-right  */
        { 1, -1}   /* diag down-left   */
    };

    for (int k = 0; k < 4; k++) {
        int dr = D[k][0];
        int dc = D[k][1];

        int total = 1
                    + ray_count(b, r, c,  dr,  dc, p)
                    + ray_count(b, r, c, -dr, -dc, p);

        if (total >= 4) {
            return true;
        }
    }

    return false;
}

/*
 * board_is_full
 * -------------
 * Return true if no more pieces can be dropped (all columns are full).
 */
bool board_is_full(const Board *b) {
    for (int c = 0; c < COLS; c++) {
        if (b->heights[c] < ROWS) {
            return false;
        }
    }
    return true;
}

/*
 * board_print
 * -----------
 * Print the board to stdout, with color for each player piece.
 */
void board_print(const Board *b) {
    /* Top border */
    printf("   +");
    for (int c = 0; c < COLS; c++) {
        printf("---+");
    }
    printf("\n");

    /* Board rows (top to bottom) */
    for (int r = 0; r < ROWS; r++) {
        printf("   |");
        for (int c = 0; c < COLS; c++) {
            Cell cell = b->grid[r][c];
            char ch = (char)cell;

            const char *start = "";
            const char *end   = "";

            if (cell == CELL_A) {
                start = ANSI_A;
                end   = ANSI_RESET;
            } else if (cell == CELL_B) {
                start = ANSI_B;
                end   = ANSI_RESET;
            }

            if (*start) {
                printf(" %s%c%s |", start, ch, end);
            } else {
                printf(" %c |", ch);
            }
        }
        printf("\n");

        /* Row separator */
        printf("   +");
        for (int c = 0; c < COLS; c++) {
            printf("---+");
        }
        printf("\n");
    }

    /* Column labels (1..COLS) */
    printf("    ");
    for (int c = 1; c <= COLS; c++) {
        printf(" %d  ", c);
    }
    printf("\n");
}

