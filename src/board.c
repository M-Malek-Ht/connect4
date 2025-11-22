#include "board.h"
#include <stdio.h>

// board.c
#define ANSI_RESET  "\x1b[0m"
#define ANSI_A      "\x1b[36m" // cyan for A
#define ANSI_B      "\x1b[35m" // magenta for B




void board_init(Board *b) {
    for (int r=0; r<ROWS; ++r)
        for (int c=0; c<COLS; ++c)
            b->grid[r][c] = CELL_EMPTY;
    for (int c=0; c<COLS; ++c) b->heights[c]=0;
}

bool board_drop(Board *b, int col1_based, Cell piece, int *out_row) {
    int c = col1_based - 1;
    if (c < 0 || c >= COLS) return false;
    if (b->heights[c] >= ROWS) return false;
    int r = ROWS - 1 - b->heights[c];  // FIX


    b->grid[r][c] = piece;
    b->heights[c]++;
    if (out_row) *out_row = r;
    return true;
}

// Count same-piece cells in one direction (excluding the origin).
// Stops at board edge or first mismatch. Max needed is 3 steps.
static int ray_count(const Board *b, int r, int c, int dr, int dc, Cell p) {
    int cnt = 0;
    for (int i = 1; i < 4; i++) {
        int rr = r + dr * i;
        int cc = c + dc * i;
        if (rr < 0 || rr >= ROWS || cc < 0 || cc >= COLS) break;
        if (b->grid[rr][cc] != p) break;
        cnt++;
    }
    return cnt;
}

bool board_is_winning(const Board *b, int r, int c, Cell p) {
    // Four principal lines to check: horizontal, vertical, two diagonals
    const int D[4][2] = { {0,1}, {1,0}, {1,1}, {1,-1} };
    for (int k = 0; k < 4; k++) {
        int dr = D[k][0], dc = D[k][1];
        int total = 1
                  + ray_count(b, r, c,  dr,  dc, p)   // forward
                  + ray_count(b, r, c, -dr, -dc, p);  // backward
        if (total >= 4) return true;
    }
    return false;
}
bool board_is_full(const Board *b) {
    for (int c=0;c<COLS;c++) if (b->heights[c] < ROWS) return false;
    return true;
}

void board_print(const Board *b) {
    // Top border
    printf("   +");
    for (int c = 0; c < COLS; c++) printf("---+");
    printf("\n");

    // Rows (top -> bottom)
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


        // Row separator
        printf("   +");
        for (int c = 0; c < COLS; c++) printf("---+");
        printf("\n");
    }

    // Column labels (1..7)
    printf("    ");
    for (int c = 1; c <= COLS; c++) printf(" %d  ", c);
    printf("\n");
}


