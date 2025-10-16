#include "game.h"
#include <stdio.h>

#include <stdlib.h>   // rand, srand
#include <time.h>     // time

#include <pthread.h>
#include <string.h>   // memcpy


// Read a column number 1..7 or 'q' to quit.
// Returns 1 if a valid column was read into *out_col,
// returns 0 if the user asked to quit (q/Q or EOF).
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

// Ensure RNG is initialized once.
static void rng_init_once(void) {
    static int init = 0;
    if (!init) { srand((unsigned)time(NULL)); init = 1; }
}

// Collect all valid columns into cols[], return count.
// Each column is 1..7 as seen by the user.
static int collect_valid_columns(const Board *b, int cols_out[COLS]) {
    int n = 0;
    for (int c = 1; c <= COLS; c++) {
        int h = b->heights[c-1];           // column height 0..ROWS
        if (h < ROWS) cols_out[n++] = c;   // column has space
    }
    return n;
}

static void board_clone(Board *dst, const Board *src) {
    memcpy(dst, src, sizeof(*dst));
}

// Simulate a move for 'p' in column 'col' and check if it wins.
static int would_win_if_drop(const Board *b, int col, Cell p) {
    Board tmp;
    memcpy(&tmp, b, sizeof(tmp));
    int placed_row;
    if (!board_drop(&tmp, col, p, &placed_row)) return 0;
    return board_is_winning(&tmp, placed_row, col - 1, p);
}

// Return opponent's immediate winning columns (write 1..7 into out[], return count).
static int opponent_winning_cols(const Board *b, Cell opponent, int out[COLS]) {
    int n = 0;
    for (int col = 1; col <= COLS; col++) {
        if (b->heights[col - 1] >= ROWS) continue;     // must be playable now
        if (would_win_if_drop(b, col, opponent)) {
            out[n++] = col;
        }
    }
    return n;
}

static int bot_pick_easy_plus(const Board *b, Cell bot_player) {
    // 1) Block immediate opponent win if any
    Cell opp = (bot_player == CELL_A) ? CELL_B : CELL_A;
    int danger[COLS];
    int dn = opponent_winning_cols(b, opp, danger);
    if (dn > 0) {
        return danger[0];   // block one of the threats
    }

    // 2) Prefer center columns if available
    static const int pref[COLS] = {4, 3, 5, 2, 6, 1, 7};
    for (int i = 0; i < COLS; i++) {
        int c = pref[i];
        if (b->heights[c - 1] < ROWS) return c;
    }

    return -1; // no moves
}

// Easy bot: pick a random valid column uniformly.
static int bot_pick_easy(const Board *b) {
    int cols[COLS];
    int n = collect_valid_columns(b, cols);
    if (n == 0) return -1;           // no moves (board full)
    rng_init_once();
    int k = rand() % n;
    return cols[k];                  // 1..7
}

typedef struct {
    Board snapshot;
    BotDifficulty diff;
    Cell bot_player;  
    int result_col;  // 1..7, or -1 if no move
} BotTask;

static int bot_pick_dispatch(const Board *b, BotDifficulty d, Cell bot_player) {
    switch (d) {
        case BOT_EASY:
        default:
            return bot_pick_easy_plus(b, bot_player);
    }
}


static void* bot_thread_main(void *arg) {
    BotTask *t = (BotTask*)arg;
    t->result_col = bot_pick_dispatch(&t->snapshot, t->diff, t->bot_player);
    return NULL;
}


Cell game_run(void) {
    Board b; board_init(&b);
    Cell turn = CELL_A;

    GameMode mode = MODE_PVP;
    BotDifficulty diff = BOT_EASY;

      // --- Mode & Difficulty selection (loop until a playable setup) ---
    while (1) {
        // Mode selection
        while (1) {
            printf("Select mode:\n");
            printf("  1) Human vs Human\n");
            printf("  2) Human vs Bot\n");
            printf("Choice: ");
            fflush(stdout);

            int choice = 0;
            if (scanf("%d", &choice) != 1) {
                int ch; while ((ch = getchar()) != '\n' && ch != EOF) {}
                puts("Invalid input. Try again.");
                continue;
            }
            int ch; while ((ch = getchar()) != '\n' && ch != EOF) {}

            if (choice == 1) { mode = MODE_PVP; break; }
            if (choice == 2) { mode = MODE_PVB; break; }
            puts("Please choose 1 or 2.");
        }

        // Difficulty (only if PvB)
        if (mode == MODE_PVB) {
            while (1) {
                printf("Select difficulty:\n");
                printf("  1) Easy (random valid)\n");
                printf("  2) Medium (not available yet)\n");
                printf("  3) Hard (not available yet)\n");
                printf("Choice: ");
                fflush(stdout);

                int choice = 0;
                if (scanf("%d", &choice) != 1) {
                    int ch; while ((ch = getchar()) != '\n' && ch != EOF) {}
                    puts("Invalid input. Try again.");
                    continue;
                }
                int ch; while ((ch = getchar()) != '\n' && ch != EOF) {}

                if (choice == 1) { diff = BOT_EASY; break; }
                if (choice == 2 || choice == 3) {
                    puts("Bot difficulty not available yet. Returning to mode selection...");
                    // go back to outer loop to re-choose PvP or PvB
                    goto RESELECT_MODE;
                }
                puts("Please choose 1, 2, or 3.");
            }
        }

        // If we got here, we have a playable setup (PvP or PvB Easy)
        break;

RESELECT_MODE:
        ; // label target requires a statement; this empty statement is fine
    }


    // --- main loop (IMPORTANT: this is outside the difficulty block) ---
    while (1) {
        printf("\nCurrent board:\n");
        board_print(&b);

        int col = -1;     // 1..7
        int placed_row;   // 0..5

        if (mode == MODE_PVP ||
            (mode == MODE_PVB && turn == CELL_A)) {
            // HUMAN turn (A in PvB, or A/B in PvP)
            printf("Player %c, ", (char)turn);
            if (!read_column_or_quit(&col)) {
                // user quit / EOF
                return CELL_EMPTY;
            }
        } else {
            // BOT turn (B when PvB) — compute in a separate thread
BotTask task;
board_clone(&task.snapshot, &b);   // read-only snapshot to avoid races
task.diff = diff;
task.bot_player = turn; 
task.result_col = -1;

pthread_t th;
if (pthread_create(&th, NULL, bot_thread_main, &task) != 0) {
    // Fallback: single-threaded if thread creation fails
    col = bot_pick_dispatch(&b, diff, turn);
} else {
    // (Optional) tiny wait UX could be added later; for now, just join.
    pthread_join(th, NULL);
    col = task.result_col;
}

if (col < 1) {
    // No valid moves (should imply draw)
    if (board_is_full(&b)) {
        printf("\nFinal board:\n");
        board_print(&b);
        puts("It's a draw.");
        return CELL_EMPTY;
    }
    // Fallback: reprompt (shouldn't happen)
    continue;
}
printf("Bot (%c) chooses column %d\n", (char)turn, col);

        }

        // Attempt to drop; if the selected column is full, retry same player.
        if (!board_drop(&b, col, turn, &placed_row)) {
            puts("That column is full or invalid. Try another.");
            continue;
        }

        // Win / Draw checks
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

        // Next player's turn
        turn = (turn == CELL_A) ? CELL_B : CELL_A;
    }
}

