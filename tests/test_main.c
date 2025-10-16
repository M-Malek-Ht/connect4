// test_main.c
#include <assert.h>
#include <stdio.h>
#include "board.h"

// Small helper: drop at 1-based column 'col' for player 'p'
static int drop(Board *b, int col, Cell p, int *out_row_zero_based, int *out_col_zero_based) {
    int r1; // row as returned by board_drop (0..ROWS-1)
    if (!board_drop(b, col, p, &r1)) return 0;
    if (out_row_zero_based) *out_row_zero_based = r1;
    if (out_col_zero_based) *out_col_zero_based = col - 1;
    return 1;
}

static void test_vertical_win(void) {
    Board b; board_init(&b);
    int r, c;

    // Four A's stacked in column 1
    assert(drop(&b, 1, CELL_A, &r, &c));
    assert(drop(&b, 1, CELL_A, &r, &c));
    assert(drop(&b, 1, CELL_A, &r, &c));
    assert(drop(&b, 1, CELL_A, &r, &c));
    assert(board_is_winning(&b, r, c, CELL_A));
}

static void test_horizontal_win(void) {
    Board b; board_init(&b);
    int r, c;

    // Bottom row horizontal A A A A at cols 1..4
    assert(drop(&b, 1, CELL_A, &r, &c));
    assert(drop(&b, 2, CELL_A, &r, &c));
    assert(drop(&b, 3, CELL_A, &r, &c));
    assert(drop(&b, 4, CELL_A, &r, &c));
    assert(board_is_winning(&b, r, c, CELL_A));
}

static void test_diag_slash_win(void) {
    // Build a diagonal like this (bottom to top): positions
    // (row,col) zero-based, using 1-based col for drops:
    //   Place to form A’s at:
    //   (5,0), (4,1), (3,2), (2,3)

    Board b; board_init(&b);
    int r, c;

    // Column 1: put A at bottom
    assert(drop(&b, 1, CELL_A, &r, &c)); // (5,0)

    // Column 2: stack one B, then A
    assert(drop(&b, 2, CELL_B, NULL, NULL));       // (5,1)
    assert(drop(&b, 2, CELL_A, &r, &c));           // (4,1)

    // Column 3: stack B,B then A
    assert(drop(&b, 3, CELL_B, NULL, NULL));       // (5,2)
    assert(drop(&b, 3, CELL_B, NULL, NULL));       // (4,2)
    assert(drop(&b, 3, CELL_A, &r, &c));           // (3,2)

    // Column 4: stack B,B,B then A
    assert(drop(&b, 4, CELL_B, NULL, NULL));       // (5,3)
    assert(drop(&b, 4, CELL_B, NULL, NULL));       // (4,3)
    assert(drop(&b, 4, CELL_B, NULL, NULL));       // (3,3)
    assert(drop(&b, 4, CELL_A, &r, &c));           // (2,3)

    // The last placed A should complete the '/' diagonal
    assert(board_is_winning(&b, r, c, CELL_A));
}

int main(void) {
    test_vertical_win();
    test_horizontal_win();
    test_diag_slash_win();
    puts("All tests passed.");
    return 0;
}

