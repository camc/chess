#ifndef FRONTEND_STATE_H
#define FRONTEND_STATE_H

#include <chess.h>
#include <config.h>
#include <stdbool.h>
#include <stdlib.h>

// Frontend state stores information needed by the frontend
// to the chess implementation (the UI, input handlers, etc)
struct FrontendState {
    struct GameState *game_state;       // The current gamestate, NULL if there is none
    struct BoardPos selected_position;  // The current position on the board selected by the user
    bool two_player_mode;               // Whether the game is two player or vs computer. True if two player.
    char *move_log;                     // The move log content
    int move_log_size;                  // The size of the move_log buffer
    int move_log_idx;                   // The current index in the move_log buffer
    int move_log_line_chars;            // The number of chars on the current line of the move log
    int winner;  // The winner of the game or -1 if it is ongoing. 0, 1, 2 for white win, black win and draw
    const char *message_box;  // The content of a message box to be displayed on screen, NULL if no message.

    // Debug settings
    bool debug_allow_illegal_moves;   // Disables legality checking of human moves
    bool debug_copy_on_move;          // Instead of moving pieces copy them
    bool debug_computer_vs_computer;  // Enables computer vs computer mode
};

// There is a single static instance of FrontendState for the entire program,
// defined in frontend_state.c
extern struct FrontendState frontend_state;

void frontend_new_game();
bool frontend_new_game_from_fen(const char *fen);
void log_move(struct BoardPos from, struct BoardPos to);

#endif /* FRONTEND_STATE_H */
