#include "board.h"
#include <stdio.h>

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

static int count_dir(const Board *b, int r, int c, int dr, int dc, Cell p) {
    int cnt=0;
    for (int i=0;i<4;i++) {
        int rr=r + dr*i, cc=c + dc*i;
        if (rr<0||rr>=ROWS||cc<0||cc>=COLS) break;
        if (b->grid[rr][cc]!=p) break;
        cnt++;
    }
    return cnt;
}

bool board_is_winning(const Board *b, int r, int c, Cell p) {

    const int dirs[4][2]={{0,1},{1,0},{1,1},{1,-1}};
    for (int k=0;k<4;k++){
        int dr=dirs[k][0], dc=dirs[k][1];
        int total=1;

        for (int i=1;i<4;i++){
            int rr=r-dr*i, cc=c-dc*i;
            if (rr<0||rr>=ROWS||cc<0||cc>=COLS) break;
            if (b->grid[rr][cc]!=p) break;
            total++;
        }

        for (int i=1;i<4;i++){
            int rr=r+dr*i, cc=c+dc*i;
            if (rr<0||rr>=ROWS||cc<0||cc>=COLS) break;
            if (b->grid[rr][cc]!=p) break;
            total++;
        }
        if (total>=4) return true;
    }
    return false;
}

bool board_is_full(const Board *b) {
    for (int c=0;c<COLS;c++) if (b->heights[c] < ROWS) return false;
    return true;
}

void board_print(const Board *b) {
    for (int r=0;r<ROWS;r++){
        for (int c=0;c<COLS;c++) printf("%c ", (char)b->grid[r][c]);
        printf("\n");
    }
    for (int c=1;c<=COLS;c++) printf("%d ", c);
    printf("\n");
}

