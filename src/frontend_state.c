#include <config.h>
#include <fen.h>
#include <frontend_state.h>
#include <string.h>
#include <tptable.h>

// The single instance of FrontendState used throughout the program
// initialised to default values
struct FrontendState frontend_state = {.game_state = NULL,
                                       .selected_position = {0xf, 0xf},  // NULL_BOARDPOS
                                       .two_player_mode = true,
                                       .move_log = NULL,
                                       .move_log_size = 0,
                                       .move_log_idx = 0,
                                       .move_log_line_chars = 0,
                                       .winner = -1,
                                       .message_box = NULL,
                                       .debug_allow_illegal_moves = false,
                                       .debug_copy_on_move = false,
                                       .debug_computer_vs_computer = false};

// Resets the parts of the frontend state used to store
// data about the current game to the default values.
// Deallocating structures if necessary.
// Other data not tied to a specific game, such as the desired game mode
// and debug settings are not changed.
static void reset_ingame_frontend_state() {
    if (frontend_state.game_state != NULL) {
        deinit_gamestate(frontend_state.game_state);
    }

    if (frontend_state.move_log != NULL) {
        free(frontend_state.move_log);
    }

    frontend_state.selected_position = NULL_BOARDPOS;
    frontend_state.move_log = NULL;
    frontend_state.move_log_size = 0;
    frontend_state.move_log_idx = 0;
    frontend_state.move_log_line_chars = 0;
    frontend_state.winner = -1;
    tptable_clear();
}

// Sets the frontend state to the default values at the start of a chess game,
// deallocating existing structures if neccesary
void frontend_new_game() {
    reset_ingame_frontend_state();
    frontend_state.game_state = init_gamestate();
}

// Sets the frontend state to defaults with a game state parsed from FEN
// deallocating existing structures if neccesary.
// Returns false if there was an error in the FEN, true otherwise
bool frontend_new_game_from_fen(const char *fen) {
    reset_ingame_frontend_state();

    frontend_state.game_state = fen_to_gamestate(fen);
    return frontend_state.game_state != NULL;
}

// Adds a move to the move log
// Must be called while in a game (frontend_new_game has been called)
void log_move(struct BoardPos from, struct BoardPos to) {
    // Extend the move log if needed
    if (frontend_state.move_log_size <= frontend_state.move_log_idx + 9) {
        frontend_state.move_log_size += 18;
        frontend_state.move_log = realloc(frontend_state.move_log, frontend_state.move_log_size);
    }

    // Convert the move to a string
    char move[5];
    int move_char_count = move_to_str(frontend_state.game_state, from, to, move);

    // If there is not enough space for the move on the current line
    // insert a newline
    if (frontend_state.move_log_line_chars + move_char_count >= 44) {
        frontend_state.move_log[frontend_state.move_log_idx++] = '\n';
        frontend_state.move_log_line_chars = 0;
    }

    // Add the move to the log
    memcpy(frontend_state.move_log + frontend_state.move_log_idx, move, move_char_count);
    frontend_state.move_log_idx += move_char_count;

    // Add a space
    frontend_state.move_log[frontend_state.move_log_idx++] = ' ';
    frontend_state.move_log_line_chars += move_char_count + 1;

    frontend_state.move_log[frontend_state.move_log_idx] = '\0';
}