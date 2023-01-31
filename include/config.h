#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

// Chess board width/height (pixels)
#define BOARD_SIZE 504
#define BOARD_SQUARE_SIZE (int)(BOARD_SIZE / 8)

// The size of the window displaying the chess board and GUI
#define WINDOW_WIDTH 896
#define WINDOW_HEIGHT 504

// The size of the transposition table
#define TRANSPOSITION_TABLE_SIZE 1048576

// The board square background colours, hex RGBA
#define LIGHT_SQUARE_COLOUR 0xfffedbff
#define DARK_SQUARE_COLOUR 0x38a3beff

// The maximum number of seconds allowed for move generation threads per move.
#define MAX_MOVEGEN_SEARCH_TIME 3

// The maximum movegen search depth attempted
// Higher values will increase memory usage.
#define MAX_SEARCH_DEPTH 32

// Defined if debug settings such as keybindings should be enabled
#if !defined(NDEBUG) || defined(CHESS_ENABLE_DEBUG_KEYS)
#define DEBUG_SETTINGS_ENABLED
#endif

#endif /* CONFIG_H */
