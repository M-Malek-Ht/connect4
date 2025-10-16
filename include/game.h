#ifndef GAME_H
#define GAME_H

#include "board.h"

typedef enum {
    MODE_PVP = 1,
    MODE_PVB = 2
} GameMode;

typedef enum {
    BOT_EASY = 1,
    BOT_MEDIUM = 2,   // reserved for later
    BOT_HARD = 3      // reserved for later
} BotDifficulty;

/**
 * Runs the game interactively.
 * Prompts for game mode (PvP or PvB) and difficulty (for PvB).
 * Returns the winning Cell (CELL_A or CELL_B), or CELL_EMPTY on draw/quit.
 */
Cell game_run(void);

#endif // GAME_H

