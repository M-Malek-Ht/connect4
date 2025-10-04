#include "game.h"
#include <stdio.h>

Cell game_run(void) {
    Board b; board_init(&b);
    Cell turn = CELL_A;

    while (1) {
        board_print(&b);
        printf("Player %c, choose a column (1-7): ", (char)turn);
        fflush(stdout);

        int col;
        if (scanf("%d", &col)!=1) return CELL_EMPTY; // EOF or invalid stream

        int placed_row=-1;
        if (!board_drop(&b, col, turn, &placed_row)) {
            printf("Invalid move.\n");
            continue; // ask same player again
        }

        if (board_is_winning(&b, placed_row, col-1, turn)) {
            board_print(&b);
            printf("Player %c wins.\n", (char)turn);
            return turn;
        }
        if (board_is_full(&b)) {
            board_print(&b);
            printf("Draw.\n");
            return CELL_EMPTY;
        }
        turn = (turn==CELL_A) ? CELL_B : CELL_A;
    }
}
