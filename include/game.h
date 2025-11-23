#ifndef GAME_H
#define GAME_H

#include "board.h"

#define MAX_MOVES           42  // 6 * 7
#define MAX_UNDO_PER_PLAYER 3

/*
 * GameMode
 * --------
 * Chooses how the game is played.
 */
typedef enum {
    MODE_PVP   = 1,  // Player vs Player
    MODE_PVB   = 2,  // Player vs Bot
    MODE_ONLINE= 3   // Reserved for online mode
} GameMode;

/*
 * BotDifficulty
 * -------------
 * Difficulty levels for the bot.
 */
typedef enum {
    BOT_EASY   = 1,
    BOT_MEDIUM = 2,
    BOT_HARD   = 3   // Reserved for later
} BotDifficulty;

/*
 * game_run
 * --------
 * Runs one full game:
 *  - asks for game mode and (if needed) bot difficulty
 *  - plays turns until win or draw
 *
 * Returns:
 *  - CELL_A or CELL_B on win
 *  - CELL_EMPTY on draw or early quit
 */
Cell game_run(void);

#endif /* GAME_H */

