#include "game.h"
#include <stdio.h>

static int read_column_or_quit(int *out_col) {
    int ch;

    while (1) {
        printf("Choose a column (1-7, or q to quit): ");
        fflush(stdout);

        ch = getchar();
        if (ch == EOF) {
            puts("\nEOF. Exiting.");
            return 0;
        }
        if (ch == 'q' || ch == 'Q') {
            puts("Quitting.");
            // consume the rest of the line if any
            while (ch != '\n' && ch != EOF) ch = getchar();
            return 0;
        }
        if (ch == '\n' || ch == '\r') {
            // empty line; reprompt
            continue;
        }

        // put the first non-newline char back and try scanf
        ungetc(ch, stdin);

        int col;
        if (scanf("%d", &col) == 1) {
            // consume the rest of the line
            do { ch = getchar(); } while (ch != '\n' && ch != EOF);

            if (col >= 1 && col <= 7) {
                *out_col = col;
                return 1;
            } else {
                puts("Please enter a number between 1 and 7.");
                continue;
            }
        }

        // Bad token: clear until end of line and reprompt
        puts("Invalid input. Type a number 1-7, or q to quit.");
        do { ch = getchar(); } while (ch != '\n' && ch != EOF);
    }
}

Cell game_run(void) {
    Board b; board_init(&b);
    Cell turn = CELL_A;

    while (1) {
        printf("\nCurrent board:\n");
board_print(&b);

printf("Player %c, ", (char)turn);
int col;
if (!read_column_or_quit(&col)) {
    // User quit or EOF
    return CELL_EMPTY;
}

// Try to drop; if invalid column/full column, keep the same player and reprompt.
int placed_row;
if (!board_drop(&b, col, turn, &placed_row)) {
    puts("That column is full or invalid. Try another.");
    continue; // same player's turn again
}

// Check win/draw as before
if (board_is_winning(&b, placed_row, col-1, turn)) {
    printf("\nFinal board:\n");
    board_print(&b);
    printf("Player %c wins!\n", (char)turn);
    return turn;
}
if (board_is_full(&b)) {
    printf("\nFinal board:\n");
    board_print(&b);
    puts("It's a draw.");
    return CELL_EMPTY;
}

// next player's turn
turn = (turn == CELL_A) ? CELL_B : CELL_A;
    }
}

