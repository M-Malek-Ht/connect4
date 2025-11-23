#define _XOPEN_SOURCE 700

#include "game.h"
#include <stdio.h>
#include <stdlib.h>    // rand, srand
#include <time.h>      // time
#include <limits.h>    // INT_MIN
#include <pthread.h>
#include <string.h>    // memcpy, strlen, strcmp, etc.
#include <unistd.h>    // usleep, close
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>     // gethostbyname

/* Forward declarations for the minimax-based evaluation. */
static int evaluate_board(const Board *b, Cell me);
static int minimax_ab(const Board *b, int depth, int alpha, int beta,
                      Cell bot, Cell current_player, int last_row, int last_col);

/* ------------------------------------------------------------------------- */
/* Basic input / utility helpers                                             */
/* ------------------------------------------------------------------------- */

/* Initialize the random number generator once per program run. */
static void rng_init_once(void) {
    static int init = 0;
    if (!init) {
        srand((unsigned)time(NULL));
        init = 1;
    }
}

// Read a column number 1..7, 'h' for hint, 'u' for undo, or 'q' to quit.
// Returns 1 if a command/column was read into *out_col,
// returns 0 if the user asked to quit (q/Q or EOF).
// Special values in *out_col:
//   -1 => hint
//   -2 => undo
static int read_column_or_quit(int *out_col) {
    int ch;

    while (1) {
        printf("Choose a column (1-7, h for hint, u for undo, or q to quit): ");
        fflush(stdout);

        ch = getchar();
        if (ch == EOF) {
            puts("\nEOF. Exiting.");
            return 0;
        }

        if (ch == 'q' || ch == 'Q') {
            puts("Quitting.");
            while (ch != '\n' && ch != EOF) ch = getchar();
            return 0;
        }

        if (ch == 'h' || ch == 'H') {
            while (ch != '\n' && ch != EOF) ch = getchar();
            *out_col = -1;  // hint
            return 1;
        }

        if (ch == 'u' || ch == 'U') {
            while (ch != '\n' && ch != EOF) ch = getchar();
            *out_col = -2;  // undo
            return 1;
        }

        if (ch == '\n' || ch == '\r') {
            continue;
        }

        // Put back first non-newline char and try reading an integer.
        ungetc(ch, stdin);

        int col;
        if (scanf("%d", &col) == 1) {
            do { ch = getchar(); } while (ch != '\n' && ch != EOF);

            if (col >= 1 && col <= 7) {
                *out_col = col;
                return 1;
            } else {
                puts("Please enter a number between 1 and 7.");
                continue;
            }
        }

        puts("Invalid input. Type a number 1-7, h for hint, u for undo, or q to quit.");
        do { ch = getchar(); } while (ch != '\n' && ch != EOF);
    }
}


/* Collect all currently playable columns (1..7). */
static int collect_valid_columns(const Board *b, int cols_out[COLS]) {
    int n = 0;
    for (int c = 1; c <= COLS; c++) {
        int h = b->heights[c - 1];
        if (h < ROWS) {
            cols_out[n++] = c;
        }
    }
    return n;
}

/* Copy a board struct. */
static void board_clone(Board *dst, const Board *src) {
    memcpy(dst, src, sizeof(*dst));
}

/* Simulate dropping p in column col and test if that move wins. */
static int would_win_if_drop(const Board *b, int col, Cell p) {
    Board tmp;
    memcpy(&tmp, b, sizeof(tmp));
    int placed_row;
    if (!board_drop(&tmp, col, p, &placed_row)) {
        return 0;
    }
    return board_is_winning(&tmp, placed_row, col - 1, p);
}

/* List all columns where the opponent would win immediately. */
static int opponent_winning_cols(const Board *b, Cell opponent, int out[COLS]) {
    int n = 0;
    for (int col = 1; col <= COLS; col++) {
        if (b->heights[col - 1] >= ROWS) {
            continue;
        }
        if (would_win_if_drop(b, col, opponent)) {
            out[n++] = col;
        }
    }
    return n;
}

/* ------------------------------------------------------------------------- */
/* Simple bot strategies (easy-plus)                                   */
/* ------------------------------------------------------------------------- */


/* Easy+ bot: first block immediate wins, then prefer center columns. */
static int bot_pick_easy_plus(const Board *b, Cell bot_player) {
    Cell opp = (bot_player == CELL_A) ? CELL_B : CELL_A;

    int danger[COLS];
    int dn = opponent_winning_cols(b, opp, danger);
    if (dn > 0) {
        return danger[0];
    }

    static const int pref[COLS] = {4, 3, 5, 2, 6, 1, 7};
    for (int i = 0; i < COLS; i++) {
        int c = pref[i];
        if (b->heights[c - 1] < ROWS) {
            return c;
        }
    }

    return -1;
}

/* ------------------------------------------------------------------------- */
/* Medium bot helpers (pattern / threat based)                               */
/* ------------------------------------------------------------------------- */

/* Immediate win for p? Return column 1..7 or -1. */
static int find_self_win_in_1(const Board *b, Cell p) {
    for (int col = 1; col <= COLS; ++col) {
        if (b->heights[col - 1] >= ROWS) continue;
        if (would_win_if_drop(b, col, p)) return col;
    }
    return -1;
}

/* Test if dropping in col for bot_player avoids giving opponent win-in-1. */
static int move_is_safe_for(const Board *b, int col, Cell bot_player) {
    Board tmp;
    memcpy(&tmp, b, sizeof(tmp));

    int placed_row;
    if (!board_drop(&tmp, col, bot_player, &placed_row)) {
        return 0;
    }

    Cell opp = (bot_player == CELL_A) ? CELL_B : CELL_A;
    int threats[COLS];
    return opponent_winning_cols(&tmp, opp, threats) == 0;
}

/* Count a contiguous line through (r,c) along (dr,dc). */
static int line_len_from(const Board *b, int r, int c, Cell p, int dr, int dc) {
    int cnt = 1;

    for (int i = 1; i < 4; i++) {
        int rr = r + dr * i;
        int cc = c + dc * i;
        if (rr < 0 || rr >= ROWS || cc < 0 || cc >= COLS) break;
        if (b->grid[rr][cc] != p) break;
        cnt++;
    }

    for (int i = 1; i < 4; i++) {
        int rr = r - dr * i;
        int cc = c - dc * i;
        if (rr < 0 || rr >= ROWS || cc < 0 || cc >= COLS) break;
        if (b->grid[rr][cc] != p) break;
        cnt++;
    }

    return cnt;
}

/* Count open three-in-a-row patterns that include (r,c). */
static int open_three_through(const Board *b, int r, int c, Cell p) {
    static const int D[4][2] = { {0,1}, {1,0}, {1,1}, {1,-1} };
    int total = 0;

    for (int k = 0; k < 4; k++) {
        int dr = D[k][0];
        int dc = D[k][1];

        for (int s = -3; s <= 0; s++) {
            int cnt = 0;
            int has_me = 0;
            int openL = 0;
            int openR = 0;

            for (int i = 0; i < 4; i++) {
                int rr = r + (s + i) * dr;
                int cc = c + (s + i) * dc;

                if (rr < 0 || rr >= ROWS || cc < 0 || cc >= COLS) {
                    cnt = -99;
                    break;
                }

                Cell q = b->grid[rr][cc];
                if (rr == r && cc == c) {
                    has_me = 1;
                }
                if (q == p) {
                    cnt++;
                } else if (q != CELL_EMPTY) {
                    cnt = -99;
                    break;
                }
            }

            if (cnt == 3 && has_me) {
                int Lr = r + (s - 1) * dr;
                int Lc = c + (s - 1) * dc;
                int Rr = r + (s + 4) * dr;
                int Rc = c + (s + 4) * dc;

                if (Lr >= 0 && Lr < ROWS && Lc >= 0 && Lc < COLS &&
                    b->grid[Lr][Lc] == CELL_EMPTY) {
                    openL = 1;
                }
                if (Rr >= 0 && Rr < ROWS && Rc >= 0 && Rc < COLS &&
                    b->grid[Rr][Rc] == CELL_EMPTY) {
                    openR = 1;
                }

                if (openL || openR) {
                    total++;
                }
            }
        }
    }

    return total;
}

/* Count how many immediate winning moves 'me' has. */
static int count_our_immediate_wins(const Board *b, Cell me) {
    int wins = 0;
    for (int col = 1; col <= COLS; ++col) {
        if (b->heights[col - 1] >= ROWS) continue;
        if (would_win_if_drop(b, col, me)) {
            wins++;
        }
    }
    return wins;
}

/* Score a hypothetical move after it has been played. */
static int score_move(const Board *after, int placed_row, int placed_col0,
                      Cell me, Cell opp, int opp_threats_before) {
    static const int D[4][2] = { {0,1}, {1,0}, {1,1}, {1,-1} };
    int best_line = 0;

    for (int k = 0; k < 4; k++) {
        int len = line_len_from(after, placed_row, placed_col0,
                                me, D[k][0], D[k][1]);
        if (len > best_line) {
            best_line = len;
        }
    }

    int s = 0;

    s += 100 * best_line;
    s +=  60 * open_three_through(after, placed_row, placed_col0, me);
    s +=  40 * count_our_immediate_wins(after, me);

    int opp_threats_after = 0;
    for (int col = 1; col <= COLS; ++col) {
        if (after->heights[col - 1] >= ROWS) continue;
        if (would_win_if_drop(after, col, opp)) {
            opp_threats_after++;
        }
    }

    int removed = (opp_threats_before > opp_threats_after)
                  ? (opp_threats_before - opp_threats_after)
                  : 0;
    s += 25 * removed;

    s += 5 * (ROWS - placed_row);

    rng_init_once();
    s += (rand() % 7) - 3;

    return s;
}

/* Medium bot:
   - win if possible
   - best blocking move
   - best safe move
   - otherwise best overall (even if risky) */
static int bot_pick_medium(const Board *b, Cell bot_player) {
    Cell opp = (bot_player == CELL_A) ? CELL_B : CELL_A;

    int opp_threats_before = 0;
    for (int col = 1; col <= COLS; ++col) {
        if (b->heights[col - 1] >= ROWS) continue;
        if (would_win_if_drop(b, col, opp)) {
            opp_threats_before++;
        }
    }

    for (int col = 1; col <= COLS; ++col) {
        if (b->heights[col - 1] >= ROWS) continue;
        if (would_win_if_drop(b, col, bot_player)) {
            return col;
        }
    }

    int best_block_col   = -1;
    int best_block_score = INT_MIN;

    for (int col = 1; col <= COLS; ++col) {
        if (b->heights[col - 1] >= ROWS) continue;
        if (!would_win_if_drop(b, col, opp)) continue;

        Board tmp = *b;
        int r;
        board_drop(&tmp, col, bot_player, &r);

        int sc = score_move(&tmp, r, col - 1, bot_player, opp, opp_threats_before);
        if (sc > best_block_score) {
            best_block_score = sc;
            best_block_col   = col;
        }
    }

    if (best_block_col != -1) {
        return best_block_col;
    }

    int best_col   = -1;
    int best_score = INT_MIN;

    for (int col = 1; col <= COLS; ++col) {
        if (b->heights[col - 1] >= ROWS) continue;

        Board tmp = *b;
        int r;
        if (!board_drop(&tmp, col, bot_player, &r)) {
            continue;
        }

        int unsafe = 0;
        for (int oc = 1; oc <= COLS; ++oc) {
            if (tmp.heights[oc - 1] >= ROWS) continue;
            if (would_win_if_drop(&tmp, oc, opp)) {
                unsafe = 1;
                break;
            }
        }
        if (unsafe) continue;

        int sc = score_move(&tmp, r, col - 1, bot_player, opp, opp_threats_before);
        if (sc > best_score) {
            best_score = sc;
            best_col   = col;
        }
    }

    if (best_col != -1) {
        return best_col;
    }

    best_col   = -1;
    best_score = INT_MIN;

    for (int col = 1; col <= COLS; ++col) {
        if (b->heights[col - 1] >= ROWS) continue;

        Board tmp = *b;
        int r;
        board_drop(&tmp, col, bot_player, &r);
        int sc = score_move(&tmp, r, col - 1, bot_player, opp, opp_threats_before);

        if (sc > best_score) {
            best_score = sc;
            best_col   = col;
        }
    }

    return best_col;
}

/* ------------------------------------------------------------------------- */
/* Evaluation and minimax (for the hard bot and analysis)                    */
/* ------------------------------------------------------------------------- */

/* Score a window of 4 cells from the view of 'me'. */
static int eval_window(Cell c1, Cell c2, Cell c3, Cell c4, Cell me) {
    Cell opp = (me == CELL_A) ? CELL_B : CELL_A;
    Cell cells[4] = { c1, c2, c3, c4 };

    int me_count    = 0;
    int opp_count   = 0;
    int empty_count = 0;

    for (int i = 0; i < 4; i++) {
        if (cells[i] == me) {
            me_count++;
        } else if (cells[i] == opp) {
            opp_count++;
        } else if (cells[i] == CELL_EMPTY) {
            empty_count++;
        }
    }

    if (me_count > 0 && opp_count > 0) {
        return 0;
    }

    int score = 0;

    if (me_count == 3 && empty_count == 1)      score += 100;
    else if (me_count == 2 && empty_count == 2) score += 10;
    else if (me_count == 1 && empty_count == 3) score += 1;

    if (opp_count == 3 && empty_count == 1)      score -= 120;
    else if (opp_count == 2 && empty_count == 2) score -= 8;
    else if (opp_count == 1 && empty_count == 3) score -= 1;

    return score;
}

/* Heuristic board score from the view of 'me'. */
static int evaluate_board(const Board *b, Cell me) {
    Cell opp = (me == CELL_A) ? CELL_B : CELL_A;
    int score = 0;

    int center_col = COLS / 2;
    for (int r = 0; r < ROWS; r++) {
        if (b->grid[r][center_col] == me) score += 6;
        else if (b->grid[r][center_col] == opp) score -= 6;
    }

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c <= COLS - 4; c++) {
            score += eval_window(b->grid[r][c],
                                 b->grid[r][c+1],
                                 b->grid[r][c+2],
                                 b->grid[r][c+3],
                                 me);
        }
    }

    for (int c = 0; c < COLS; c++) {
        for (int r = 0; r <= ROWS - 4; r++) {
            score += eval_window(b->grid[r][c],
                                 b->grid[r+1][c],
                                 b->grid[r+2][c],
                                 b->grid[r+3][c],
                                 me);
        }
    }

    for (int r = 0; r <= ROWS - 4; r++) {
        for (int c = 0; c <= COLS - 4; c++) {
            score += eval_window(b->grid[r][c],
                                 b->grid[r+1][c+1],
                                 b->grid[r+2][c+2],
                                 b->grid[r+3][c+3],
                                 me);
        }
    }

    for (int r = 3; r < ROWS; r++) {
        for (int c = 0; c <= COLS - 4; c++) {
            score += eval_window(b->grid[r][c],
                                 b->grid[r-1][c+1],
                                 b->grid[r-2][c+2],
                                 b->grid[r-3][c+3],
                                 me);
        }
    }

    return score;
}

/* Depth-limited minimax with alpha-beta pruning. */
static int minimax_ab(const Board *b, int depth, int alpha, int beta,
                      Cell bot, Cell current, int last_row, int last_col) {
    Cell opp = (bot == CELL_A) ? CELL_B : CELL_A;

    if (last_row >= 0 && last_col >= 0) {
        Cell last_player = (current == CELL_A) ? CELL_B : CELL_A;
        if (board_is_winning(b, last_row, last_col, last_player)) {
            int base = 1000000;
            if (last_player == bot) {
                return base + depth;
            } else {
                return -base - depth;
            }
        }
    }

    if (depth == 0 || board_is_full(b)) {
        return evaluate_board(b, bot);
    }

    static const int ORDER[COLS] = {4, 3, 5, 2, 6, 1, 7};
    int best;

    if (current == bot) {
        best = INT_MIN;

        for (int i = 0; i < COLS; i++) {
            int col = ORDER[i];
            if (b->heights[col - 1] >= ROWS) continue;

            Board tmp = *b;
            int r;
            if (!board_drop(&tmp, col, current, &r)) continue;

            int val = minimax_ab(&tmp, depth - 1, alpha, beta,
                                 bot,
                                 opp,
                                 r, col - 1);

            if (val > best)  best  = val;
            if (val > alpha) alpha = val;
            if (beta <= alpha) break;
        }

        return best;
    } else {
        best = INT_MAX;

        for (int i = 0; i < COLS; i++) {
            int col = ORDER[i];
            if (b->heights[col - 1] >= ROWS) continue;

            Board tmp = *b;
            int r;
            if (!board_drop(&tmp, col, current, &r)) continue;

            int val = minimax_ab(&tmp, depth - 1, alpha, beta,
                                 bot,
                                 bot,
                                 r, col - 1);

            if (val < best) best = val;
            if (val < beta) beta = val;
            if (beta <= alpha) break;
        }

        return best;
    }
}

/* ------------------------------------------------------------------------- */
/* Hard bot: parallel minimax over moves                                    */
/* ------------------------------------------------------------------------- */

typedef struct {
    Board board;
    Cell  bot;
    Cell  opp;
    int   col;
    int   depth;
    int   score;
    int   valid;
} HardSearchTask;

static void* hard_worker_main(void *arg) {
    HardSearchTask *t = (HardSearchTask*)arg;

    if (!t->valid) {
        t->score = INT_MIN;
        return NULL;
    }

    int r;
    if (!board_drop(&t->board, t->col, t->bot, &r)) {
        t->score = INT_MIN;
        return NULL;
    }

    t->score = minimax_ab(&t->board,
                          t->depth - 1,
                          INT_MIN, INT_MAX,
                          t->bot,
                          t->opp,
                          r, t->col - 1);

    return NULL;
}

/* Hard-level bot: minimax with alpha-beta and per-column threads. */
static int bot_pick_hard(const Board *b, Cell bot_player) {
    Cell opp = (bot_player == CELL_A) ? CELL_B : CELL_A;

    int win_col = find_self_win_in_1(b, bot_player);
    if (win_col != -1) {
        return win_col;
    }

    int danger[COLS];
    int dn = opponent_winning_cols(b, opp, danger);
    if (dn > 0) {
        return danger[0];
    }

    const int MAX_DEPTH = 7;
    static const int ORDER[COLS] = {4, 3, 5, 2, 6, 1, 7};

    HardSearchTask tasks[COLS];
    pthread_t       threads[COLS];
    int             has_thread[COLS];

    for (int i = 0; i < COLS; i++) {
        tasks[i].valid = 0;
        has_thread[i]  = 0;
    }

    for (int i = 0; i < COLS; i++) {
        int col = ORDER[i];
        if (b->heights[col - 1] >= ROWS) {
            continue;
        }

        tasks[i].board = *b;
        tasks[i].bot   = bot_player;
        tasks[i].opp   = opp;
        tasks[i].col   = col;
        tasks[i].depth = MAX_DEPTH;
        tasks[i].score = INT_MIN;
        tasks[i].valid = 1;

        if (pthread_create(&threads[i], NULL, hard_worker_main, &tasks[i]) == 0) {
            has_thread[i] = 1;
        } else {
            hard_worker_main(&tasks[i]);
            has_thread[i] = 0;
        }
    }

    int best_col   = -1;
    int best_score = INT_MIN;

    for (int i = 0; i < COLS; i++) {
        if (!tasks[i].valid) continue;

        if (has_thread[i]) {
            pthread_join(threads[i], NULL);
        }

        int col   = tasks[i].col;
        int score = tasks[i].score;

        if (best_col == -1 || score > best_score) {
            best_score = score;
            best_col   = col;
        }
    }

    return best_col;
}

/* ------------------------------------------------------------------------- */
/* Bot dispatch + worker thread                                              */
/* ------------------------------------------------------------------------- */

static int bot_pick_dispatch(const Board *b, BotDifficulty d, Cell bot_player) {
    switch (d) {
        case BOT_EASY:
            return bot_pick_easy_plus(b, bot_player);
        case BOT_MEDIUM:
            return bot_pick_medium(b, bot_player);
        case BOT_HARD:
            return bot_pick_hard(b, bot_player);
        default:
            return bot_pick_easy_plus(b, bot_player);
    }
}

typedef struct {
    Board         snapshot;
    BotDifficulty diff;
    Cell          bot_player;
    int           result_col;
} BotTask;

static void* bot_thread_main(void *arg) {
    BotTask *t = (BotTask*)arg;
    t->result_col = bot_pick_dispatch(&t->snapshot, t->diff, t->bot_player);
    return NULL;
}

/* ------------------------------------------------------------------------- */
/* Post-game analysis                                                        */
/* ------------------------------------------------------------------------- */

typedef struct {
    Cell player;
    int  col;
} Move;

/* Replay the game and report missed wins and final evaluation. */
static void game_post_analysis(const Move *history, int move_count, Cell winner) {
    printf("\n=== Post-game analysis ===\n");
    printf("Total moves played: %d\n", move_count);

    Board sim;
    board_init(&sim);

    int missed_win_count = 0;

    for (int i = 0; i < move_count; ++i) {
        Cell p  = history[i].player;
        int col = history[i].col;

        int winning_col = find_self_win_in_1(&sim, p);
        if (winning_col != -1 && winning_col != col) {
            missed_win_count++;
            printf("Move %d: Player %c played column %d but had a WIN in column %d.\n",
                   i + 1, (char)p, col, winning_col);
        }

        int r;
        board_drop(&sim, col, p, &r);
    }

    if (missed_win_count == 0) {
        printf("No missed immediate winning moves detected.\n");
    }

    if (winner == CELL_A || winner == CELL_B) {
        int final_eval = evaluate_board(&sim, winner);
        printf("Final evaluation from winner's perspective: %+d (higher = more dominant).\n",
               final_eval);
    } else {
        int evalA = evaluate_board(&sim, CELL_A);
        int evalB = evaluate_board(&sim, CELL_B);
        printf("Final evaluation: A: %+d, B: %+d.\n", evalA, evalB);
    }

    printf("=== End of analysis ===\n");
}

/* ------------------------------------------------------------------------- */
/* Networking helpers (line-based TCP protocol)                              */
/* ------------------------------------------------------------------------- */

static int send_all(int sockfd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sockfd, buf + sent, len - sent, 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static int send_line(int sockfd, const char *line) {
    size_t len = strlen(line);
    return send_all(sockfd, line, len);
}

/* Read one line (ending with '\n') into buf. */
static int recv_line(int sockfd, char *buf, size_t maxlen) {
    size_t pos = 0;
    if (maxlen == 0) return -1;
    maxlen--;

    while (1) {
        char ch;
        ssize_t n = recv(sockfd, &ch, 1, 0);
        if (n == 0) {
            if (pos == 0) return 0;
            break;
        } else if (n < 0) {
            return -1;
        }

        if (ch == '\n') {
            break;
        }

        if (pos < maxlen) {
            buf[pos++] = ch;
        }
    }

    buf[pos] = '\0';
    return 1;
}

/* Create a listening TCP socket on given port. */
static int net_listen(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[ONLINE] socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[ONLINE] setsockopt");
        close(sockfd);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[ONLINE] bind");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 1) < 0) {
        perror("[ONLINE] listen");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/* Connect to host:port using IPv4 or hostname. */
static int net_connect(const char *host, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[ONLINE] socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        struct hostent *he = gethostbyname(host);
        if (!he || !he->h_addr_list[0]) {
            fprintf(stderr, "[ONLINE] Failed to resolve host '%s'\n", host);
            close(sockfd);
            return -1;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);
    }

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[ONLINE] connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/* ------------------------------------------------------------------------- */
/* Online mode: server and client                                            */
/* ------------------------------------------------------------------------- */

static Cell game_run_online_server(int port) {
    printf("[ONLINE] Hosting game on port %d...\n", port);

    int listen_fd = net_listen(port);
    if (listen_fd < 0) {
        puts("[ONLINE] Failed to set up listening socket.");
        return CELL_EMPTY;
    }

    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);
    int conn_fd = accept(listen_fd, (struct sockaddr*)&cli_addr, &cli_len);
    if (conn_fd < 0) {
        perror("[ONLINE] accept");
        close(listen_fd);
        return CELL_EMPTY;
    }

    close(listen_fd);

    char addr_str[64];
    inet_ntop(AF_INET, &cli_addr.sin_addr, addr_str, sizeof(addr_str));
    printf("[ONLINE] Client connected from %s\n", addr_str);

    Board b;
    board_init(&b);

    Cell local  = CELL_A;
    Cell remote = CELL_B;
    Cell turn   = CELL_A;

    Move history[ROWS * COLS];
    int  move_count = 0;

    while (1) {
        printf("\n[ONLINE] Current board:\n");
        board_print(&b);

        if (turn == local) {
            int col = -1;
            printf("[ONLINE] You are Player A (local).\n");
            printf("Player A, ");
            if (!read_column_or_quit(&col)) {
                puts("[ONLINE] You quit the game.");
                close(conn_fd);
                return CELL_EMPTY;
            }

            if (col < 1 || col > COLS) {
                puts("[ONLINE] Invalid column, try again.");
                continue;
            }

            int placed_row;
            if (!board_drop(&b, col, local, &placed_row)) {
                puts("[ONLINE] That column is full or invalid. Try another.");
                continue;
            }

            if (move_count < ROWS * COLS) {
                history[move_count].player = local;
                history[move_count].col    = col;
                move_count++;
            }

            char msg[32];
            snprintf(msg, sizeof(msg), "MOVE %d\n", col);
            if (send_line(conn_fd, msg) < 0) {
                puts("[ONLINE] Failed to send move. Connection lost.");
                close(conn_fd);
                return CELL_EMPTY;
            }

            if (board_is_winning(&b, placed_row, col - 1, local)) {
                printf("\n[ONLINE] Final board:\n");
                board_print(&b);
                puts("[ONLINE] You (Player A) win!");
                game_post_analysis(history, move_count, local);
                close(conn_fd);
                return local;
            }

            if (board_is_full(&b)) {
                printf("\n[ONLINE] Final board:\n");
                board_print(&b);
                puts("[ONLINE] It's a draw.");
                game_post_analysis(history, move_count, CELL_EMPTY);
                close(conn_fd);
                return CELL_EMPTY;
            }

            turn = remote;
        } else {
            puts("[ONLINE] Waiting for Player B (remote) move...");

            char buf[64];
            int  rcv = recv_line(conn_fd, buf, sizeof(buf));
            if (rcv <= 0) {
                puts("[ONLINE] Connection closed by client.");
                close(conn_fd);
                return CELL_EMPTY;
            }

            int col = -1;
            if (sscanf(buf, "MOVE %d", &col) != 1 || col < 1 || col > COLS) {
                printf("[ONLINE] Protocol error: got '%s'\n", buf);
                close(conn_fd);
                return CELL_EMPTY;
            }

            int placed_row;
            if (!board_drop(&b, col, remote, &placed_row)) {
                puts("[ONLINE] Remote sent invalid move. Aborting.");
                close(conn_fd);
                return CELL_EMPTY;
            }

            if (move_count < ROWS * COLS) {
                history[move_count].player = remote;
                history[move_count].col    = col;
                move_count++;
            }

            if (board_is_winning(&b, placed_row, col - 1, remote)) {
                printf("\n[ONLINE] Final board:\n");
                board_print(&b);
                puts("[ONLINE] Player B (remote) wins.");
                game_post_analysis(history, move_count, remote);
                close(conn_fd);
                return remote;
            }

            if (board_is_full(&b)) {
                printf("\n[ONLINE] Final board:\n");
                board_print(&b);
                puts("[ONLINE] It's a draw.");
                game_post_analysis(history, move_count, CELL_EMPTY);
                close(conn_fd);
                return CELL_EMPTY;
            }

            turn = local;
        }
    }
}

static Cell game_run_online_client(const char *host, int port) {
    printf("[ONLINE] Connecting to %s:%d ...\n", host, port);

    int sockfd = net_connect(host, port);
    if (sockfd < 0) {
        puts("[ONLINE] Failed to connect to server.");
        return CELL_EMPTY;
    }

    puts("[ONLINE] Connected.");

    Board b;
    board_init(&b);

    Cell local  = CELL_B;
    Cell remote = CELL_A;
    Cell turn   = CELL_A;

    Move history[ROWS * COLS];
    int  move_count = 0;

    while (1) {
        printf("\n[ONLINE] Current board:\n");
        board_print(&b);

        if (turn == remote) {
            puts("[ONLINE] Waiting for Player A (remote) move...");

            char buf[64];
            int  rcv = recv_line(sockfd, buf, sizeof(buf));
            if (rcv <= 0) {
                puts("[ONLINE] Connection closed by server.");
                close(sockfd);
                return CELL_EMPTY;
            }

            int col = -1;
            if (sscanf(buf, "MOVE %d", &col) != 1 || col < 1 || col > COLS) {
                printf("[ONLINE] Protocol error: got '%s'\n", buf);
                close(sockfd);
                return CELL_EMPTY;
            }

            int placed_row;
            if (!board_drop(&b, col, remote, &placed_row)) {
                puts("[ONLINE] Remote sent invalid move. Aborting.");
                close(sockfd);
                return CELL_EMPTY;
            }

            if (move_count < ROWS * COLS) {
                history[move_count].player = remote;
                history[move_count].col    = col;
                move_count++;
            }

            if (board_is_winning(&b, placed_row, col - 1, remote)) {
                printf("\n[ONLINE] Final board:\n");
                board_print(&b);
                puts("[ONLINE] Player A (remote) wins.");
                game_post_analysis(history, move_count, remote);
                close(sockfd);
                return remote;
            }

            if (board_is_full(&b)) {
                printf("\n[ONLINE] Final board:\n");
                board_print(&b);
                puts("[ONLINE] It's a draw.");
                game_post_analysis(history, move_count, CELL_EMPTY);
                close(sockfd);
                return CELL_EMPTY;
            }

            turn = local;
        } else {
            int col = -1;
            printf("[ONLINE] You are Player B (local).\n");
            printf("Player B, ");
            if (!read_column_or_quit(&col)) {
                puts("[ONLINE] You quit the game.");
                close(sockfd);
                return CELL_EMPTY;
            }

            if (col < 1 || col > COLS) {
                puts("[ONLINE] Invalid column, try again.");
                continue;
            }

            int placed_row;
            if (!board_drop(&b, col, local, &placed_row)) {
                puts("[ONLINE] That column is full or invalid. Try another.");
                continue;
            }

            if (move_count < ROWS * COLS) {
                history[move_count].player = local;
                history[move_count].col    = col;
                move_count++;
            }

            char msg[32];
            snprintf(msg, sizeof(msg), "MOVE %d\n", col);
            if (send_line(sockfd, msg) < 0) {
                puts("[ONLINE] Failed to send move. Connection lost.");
                close(sockfd);
                return CELL_EMPTY;
            }

            if (board_is_winning(&b, placed_row, col - 1, local)) {
                printf("\n[ONLINE] Final board:\n");
                board_print(&b);
                puts("[ONLINE] You (Player B) win!");
                game_post_analysis(history, move_count, local);
                close(sockfd);
                return local;
            }

            if (board_is_full(&b)) {
                printf("\n[ONLINE] Final board:\n");
                board_print(&b);
                puts("[ONLINE] It's a draw.");
                game_post_analysis(history, move_count, CELL_EMPTY);
                close(sockfd);
                return CELL_EMPTY;
            }

            turn = remote;
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Main game loop (local PvP / PvB / online dispatch)                        */
/* ------------------------------------------------------------------------- */

Cell game_run(void) {
    Move history[ROWS * COLS];
    int  move_count = 0;
  
    int undos_used_A = 0;
    int undos_used_B = 0;
      
    Board b;
    board_init(&b);

    Cell        turn = CELL_A;
    GameMode    mode = MODE_PVP;
    BotDifficulty diff = BOT_EASY;

    while (1) {
        while (1) {
            printf("Select mode:\n");
            printf("  1) Human vs Human (local)\n");
            printf("  2) Human vs Bot\n");
            printf("  3) Human vs Human (online)\n");
            printf("Choice: ");
            fflush(stdout);

            int choice = 0;
            if (scanf("%d", &choice) != 1) {
                int ch; while ((ch = getchar()) != '\n' && ch != EOF) {}
                puts("Invalid input. Try again.");
                continue;
            }
            int ch; while ((ch = getchar()) != '\n' && ch != EOF) {}

            if (choice == 1) { mode = MODE_PVP;    break; }
            if (choice == 2) { mode = MODE_PVB;    break; }
            if (choice == 3) { mode = MODE_ONLINE; break; }
            puts("Please choose 1, 2, or 3.");
        }

        if (mode == MODE_PVB) {
            while (1) {
                printf("Select difficulty:\n");
                printf("  1) Easy\n");
                printf("  2) Medium\n");
                printf("  3) Hard\n");
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
                if (choice == 3) { diff = BOT_HARD;   break; }
                puts("Please choose 1, 2, or 3.");
            }
        }

        break;

    }

    if (mode == MODE_ONLINE) {
        int role = 0;
        while (1) {
            printf("Online mode:\n");
            printf("  1) Host game (server)\n");
            printf("  2) Join game (client)\n");
            printf("Choice: ");
            fflush(stdout);

            if (scanf("%d", &role) != 1) {
                int ch; while ((ch = getchar()) != '\n' && ch != EOF) {}
                puts("Invalid input. Try again.");
                continue;
            }
            int ch; while ((ch = getchar()) != '\n' && ch != EOF) {}

            if (role == 1 || role == 2) break;
            puts("Please choose 1 or 2.");
        }

        int port = 12345;

        if (role == 1) {
            return game_run_online_server(port);
        } else {
            char host[128];
            printf("Enter server IP/hostname (default 127.0.0.1): ");
            fflush(stdout);
            if (!fgets(host, sizeof(host), stdin)) {
                puts("Input error.");
                return CELL_EMPTY;
            }

            char *nl = strchr(host, '\n');
            if (nl) *nl = '\0';
            if (host[0] == '\0') {
                strcpy(host, "127.0.0.1");
            }
            return game_run_online_client(host, port);
        }
    }

    while (1) {
        printf("\nCurrent board:\n");
        board_print(&b);

        int col         = -1;
        int placed_row  = -1;

        if (mode == MODE_PVP ||
    (mode == MODE_PVB && turn == CELL_A)) {
    // HUMAN turn (A in PvB, or A/B in PvP)
    printf("Player %c, ", (char)turn);
    if (!read_column_or_quit(&col)) {
        // user quit / EOF
        return CELL_EMPTY;
    }

    if (col == -1) {
        // Hint request: use Hard bot logic as an advisor
        int suggestion = bot_pick_hard(&b, turn);
        if (suggestion < 1) {
            puts("No hint available.");
        } else {
            printf("Hint for player %c: consider column %d.\n",
                   (char)turn, suggestion);
        }
        // Re-prompt same player (no move played).
        continue;
    }

    if (col == -2) {
        // Undo the most recent move (if any), respecting per-player limits.
        if (move_count == 0) {
            puts("No moves to undo.");
            continue;
        }

        Move last = history[move_count - 1];
        Cell last_player = last.player;
        int  last_col    = last.col;

        int *undo_used_ptr =
            (last_player == CELL_A) ? &undos_used_A : &undos_used_B;

        if (*undo_used_ptr >= MAX_UNDO_PER_PLAYER) {
            printf("Player %c has no undos left (max %d).\n",
                   (char)last_player, MAX_UNDO_PER_PLAYER);
            continue;
        }

        int c0 = last_col - 1;
        if (c0 < 0 || c0 >= COLS || b.heights[c0] <= 0) {
            // Should not happen if board and history stay in sync
            puts("Cannot undo last move due to board state.");
            continue;
        }

        int h   = b.heights[c0];    // 1..ROWS
        int row = ROWS - h;         // row of top piece in this column

        b.grid[row][c0] = CELL_EMPTY;
        b.heights[c0]--;

        move_count--;
        (*undo_used_ptr)++;

        turn = last_player;  // give turn back to the player whose move was undone

        printf("Undo: removed last move of player %c in column %d "
               "(undos used: %d/%d).\n",
               (char)last_player, last_col,
               *undo_used_ptr, MAX_UNDO_PER_PLAYER);

        continue;
    }

} else {
    // BOT turn (B when PvB) — compute in a separate thread
    BotTask task;
    board_clone(&task.snapshot, &b);   // read-only snapshot to avoid races
    task.diff       = diff;
    task.bot_player = turn;
    task.result_col = -1;

    pthread_t th;
    if (pthread_create(&th, NULL, bot_thread_main, &task) != 0) {
        // Fallback: single-threaded if thread creation fails
        col = bot_pick_dispatch(&b, diff, turn);
    } else {
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

    // Small artificial delay so it looks like the bot is "thinking"
    usleep(150000); // 150 ms

    printf("Bot (%c) chooses column %d\n", (char)turn, col);
}


        if (!board_drop(&b, col, turn, &placed_row)) {
            puts("That column is full or invalid. Try another.");
            continue;
        }

        if (move_count < ROWS * COLS) {
            history[move_count].player = turn;
            history[move_count].col    = col;
            move_count++;
        }

        if (board_is_winning(&b, placed_row, col - 1, turn)) {
            printf("\nFinal board:\n");
            board_print(&b);
            printf("Player %c wins!\n", (char)turn);

            game_post_analysis(history, move_count, turn);
            return turn;
        }

        if (board_is_full(&b)) {
            printf("\nFinal board:\n");
            board_print(&b);
            puts("It's a draw.");

            game_post_analysis(history, move_count, CELL_EMPTY);
            return CELL_EMPTY;
        }

        turn = (turn == CELL_A) ? CELL_B : CELL_A;
    }
}

