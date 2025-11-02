#include "game.h"
#include <stdio.h>

#include <stdlib.h>   // rand, srand
#include <time.h>     // time
#include <limits.h>   // INT_MIN

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

/* ===================== Medium bot helpers (pattern-based) ===================== */

// Is there any immediate winning move for player 'p'? Return that column (1..7) or -1.
static int find_self_win_in_1(const Board *b, Cell p) {
    for (int col = 1; col <= COLS; ++col) {
        if (b->heights[col - 1] >= ROWS) continue;
        if (would_win_if_drop(b, col, p)) return col;
    }
    return -1;
}

// After bot plays 'col', does the opponent have any win-in-1 reply?
static int move_is_safe_for(const Board *b, int col, Cell bot_player) {
    Board tmp; memcpy(&tmp, b, sizeof(tmp));
    int placed_row;
    if (!board_drop(&tmp, col, bot_player, &placed_row)) return 0; // not playable => not safe
    Cell opp = (bot_player == CELL_A) ? CELL_B : CELL_A;
    int threats[COLS];
    return opponent_winning_cols(&tmp, opp, threats) == 0;
}

// counts contiguous p including (r,c) in both directions along (dr,dc)
static int line_len_from(const Board *b, int r, int c, Cell p, int dr, int dc) {
    int cnt = 1;
    for (int i=1;i<4;i++){ int rr=r+dr*i, cc=c+dc*i; if(rr<0||rr>=ROWS||cc<0||cc>=COLS) break; if(b->grid[rr][cc]!=p) break; cnt++; }
    for (int i=1;i<4;i++){ int rr=r-dr*i, cc=c-dc*i; if(rr<0||rr>=ROWS||cc<0||cc>=COLS) break; if(b->grid[rr][cc]!=p) break; cnt++; }
    return cnt;
}

// whether there exists a length-3 through (r,c) with at least one open end
static int open_three_through(const Board *b, int r, int c, Cell p) {
    static const int D[4][2]={{0,1},{1,0},{1,1},{1,-1}};
    int total = 0;
    for (int k=0;k<4;k++){
        int dr=D[k][0], dc=D[k][1];
        for (int s=-3;s<=0;s++){
            int cnt=0, has_me=0, openL=0, openR=0;
            for (int i=0;i<4;i++){
                int rr=r+(s+i)*dr, cc=c+(s+i)*dc;
                if(rr<0||rr>=ROWS||cc<0||cc>=COLS){ cnt=-99; break; }
                Cell q=b->grid[rr][cc];
                if (rr==r && cc==c) has_me=1;
                if (q==p) cnt++;
                else if (q!=CELL_EMPTY){ cnt=-99; break; }
            }
            if (cnt==3 && has_me){
                int Lr=r+(s-1)*dr, Lc=c+(s-1)*dc;
                int Rr=r+(s+4)*dr, Rc=c+(s+4)*dc;
                if (Lr>=0&&Lr<ROWS&&Lc>=0&&Lc<COLS && b->grid[Lr][Lc]==CELL_EMPTY) openL=1;
                if (Rr>=0&&Rr<ROWS&&Rc>=0&&Rc<COLS && b->grid[Rr][Rc]==CELL_EMPTY) openR=1;
                if (openL||openR) total++;
            }
        }
    }
    return total;
}

// how many win-in-1 moves do WE have after this position?
static int count_our_immediate_wins(const Board *b, Cell me){
    int wins=0;
    for(int col=1; col<=COLS; ++col){
        if (b->heights[col-1] >= ROWS) continue;
        if (would_win_if_drop(b, col, me)) wins++;
    }
    return wins;
}

static int score_move(const Board *after, int placed_row, int placed_col0, Cell me, Cell opp,
                      int opp_threats_before) {
    static const int D[4][2]={{0,1},{1,0},{1,1},{1,-1}};
    int best_line=0;
    for(int k=0;k<4;k++){
        int len=line_len_from(after, placed_row, placed_col0, me, D[k][0], D[k][1]);
        if(len>best_line) best_line=len;
    }
    int s = 0;
    s += 100 * best_line;
    s +=  60 * open_three_through(after, placed_row, placed_col0, me);
    s +=  40 * count_our_immediate_wins(after, me);

    // threats removed = opp immediate wins before - after
    int opp_threats_after=0;
    for(int col=1; col<=COLS; ++col){
        if (after->heights[col-1] >= ROWS) continue;
        if (would_win_if_drop(after, col, opp)) opp_threats_after++;
    }
    int removed = (opp_threats_before > opp_threats_after) ? (opp_threats_before - opp_threats_after) : 0;
    s += 25 * removed;

    // small “depth” bonus (gravity/stability)
    s += 5 * (ROWS - placed_row);

    // tiny jitter to avoid determinism (±3)
    rng_init_once();
    s += (rand()%7) - 3;
    return s;
}

// Medium: threat-scoring, no center bias.
// Steps: (1) win now; (2) block now (choose best blocking move by score);
// (3) among SAFE moves (do not give opponent win-next), pick highest score;
// (4) if none safe, pick highest scoring move anyway.
static int bot_pick_medium(const Board *b, Cell bot_player) {
    Cell opp = (bot_player == CELL_A) ? CELL_B : CELL_A;

    // Number of opponent immediate wins in current position
    int opp_threats_before = 0;
    for(int col=1; col<=COLS; ++col){
        if (b->heights[col-1] >= ROWS) continue;
        if (would_win_if_drop(b, col, opp)) opp_threats_before++;
    }

    // 1) Win now
    for (int col=1; col<=COLS; ++col){
        if (b->heights[col-1] >= ROWS) continue;
        if (would_win_if_drop(b, col, bot_player)) return col;
    }

    // 2) Block now — pick the block with best heuristic score
    int best_block_col = -1, best_block_score = INT_MIN;
    for (int col=1; col<=COLS; ++col){
        if (b->heights[col-1] >= ROWS) continue;
        if (!would_win_if_drop(b, col, opp)) continue; // not a block
        Board tmp = *b;
        int r;
        board_drop(&tmp, col, bot_player, &r);
        int sc = score_move(&tmp, r, col-1, bot_player, opp, opp_threats_before);
        if (sc > best_block_score){ best_block_score=sc; best_block_col=col; }
    }
    if (best_block_col != -1) return best_block_col;

    // 3) Score all SAFE moves (don’t hand opponent win-next)
    int best_col = -1, best_score = INT_MIN;
    for (int col=1; col<=COLS; ++col){
        if (b->heights[col-1] >= ROWS) continue;
        Board tmp = *b;
        int r;
        if (!board_drop(&tmp, col, bot_player, &r)) continue;

        // safety: ensure opponent has no win-in-1 reply
        int unsafe = 0;
        for(int oc=1; oc<=COLS; ++oc){
            if (tmp.heights[oc-1] >= ROWS) continue;
            if (would_win_if_drop(&tmp, oc, opp)) { unsafe = 1; break; }
        }
        if (unsafe) continue;

        int sc = score_move(&tmp, r, col-1, bot_player, opp, opp_threats_before);
        if (sc > best_score){ best_score=sc; best_col=col; }
    }
    if (best_col != -1) return best_col;

    // 4) Nothing safe? take the best overall (damage control)
    best_col = -1; best_score = INT_MIN;
    for (int col=1; col<=COLS; ++col){
        if (b->heights[col-1] >= ROWS) continue;
        Board tmp = *b;
        int r; board_drop(&tmp, col, bot_player, &r);
        int sc = score_move(&tmp, r, col-1, bot_player, opp, opp_threats_before);
        if (sc > best_score){ best_score=sc; best_col=col; }
    }
    return best_col;
}
/* ===================================================================== */

static int bot_pick_dispatch(const Board *b, BotDifficulty d, Cell bot_player) {
    switch (d) {
        case BOT_EASY:
            // Easy+ as requested (center bias + immediate block).
            return bot_pick_easy_plus(b, bot_player);
        case BOT_MEDIUM:
            // Medium: threat-scoring (win/block/safe-score), no center bias.
            return bot_pick_medium(b, bot_player);
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
                printf("  1) Easy\n");
                printf("  2) Medium\n");
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

                if (choice == 1) { diff = BOT_EASY;   break; }
                if (choice == 2) { diff = BOT_MEDIUM; break; }
                if (choice == 3) { puts("Hard not available yet."); continue; }
                puts("Please choose 1, 2, or 3.");
            }
        }

        // If we got here, we have a playable setup (PvP or PvB Easy/Medium)
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

