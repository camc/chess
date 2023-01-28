#ifdef CHESS_TEST
#include "tests/test_main.c"
#endif
#include <config.h>
#include <draw.h>
#include <engine.h>
#include <fen.h>
#include <frontend_state.h>
#include <inttypes.h>
#include <openings.h>
#include <raygui.h>
#include <raylib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tptable.h>
#include <ui.h>
#include <zobrist.h>

// Handles input and draws the chess board
// To be called every frame
// Must be called within BeginDrawing/EndDrawing
static void game_loop() {
// Enables the debug keybinds if the 'not debug' automatic cmake definition is undefined or CHESS_ENABLE_DEBUG_KEYS is
// defined
#ifdef DEBUG_SETTINGS_ENABLED
    // Toggle the next player to move
    if (IsKeyPressed(KEY_T)) {
        frontend_state.game_state->white_to_move = !frontend_state.game_state->white_to_move;
    }

    // Toggle allowing illegal moves to be made by human players
    if (IsKeyPressed(KEY_I)) {
        frontend_state.debug_allow_illegal_moves = !frontend_state.debug_allow_illegal_moves;
    }

    // Toggle copying the source piece when humans move, instead of removing it from the source
    if (IsKeyPressed(KEY_C)) {
        frontend_state.debug_copy_on_move = !frontend_state.debug_copy_on_move;
    }

    // Toggle the frontend_state.winner state between no frontend_state.winner, white wins, black wins and draw
    if (IsKeyPressed(KEY_W)) {
        frontend_state.winner = frontend_state.winner == 2 ? -1 : frontend_state.winner + 1;
    }

    // Toggle between player-vs-player and player-vs-computer game modes
    if (IsKeyPressed(KEY_M)) {
        frontend_state.two_player_mode = !frontend_state.two_player_mode;
    }

    // Toggle computer-vs-computer mode
    if (IsKeyPressed(KEY_A)) {
        frontend_state.debug_computer_vs_computer = !frontend_state.debug_computer_vs_computer;
    }

    // Print the position value relative to white
    if (IsKeyPressed(KEY_V)) {
        printf("v: %d\n", position_value(frontend_state.game_state));
    }

    // Print the zobrist hash of the state
    if (IsKeyPressed(KEY_Z)) {
        printf("z: %" PRIx64 "\n", frontend_state.game_state->hash);
    }

    // Print the FEN string of the state
    if (IsKeyPressed(KEY_F)) {
        char fen[90];
        gamestate_to_fen(frontend_state.game_state, fen);
        printf("f: %s\n", fen);
    }

#endif

    // Computer moves
    if (frontend_state.winner == -1 && !frontend_state.two_player_mode &&
        (frontend_state.debug_computer_vs_computer || !frontend_state.game_state->white_to_move)) {
        enum Player player = frontend_state.game_state->white_to_move ? WhitePlayer : BlackPlayer;
        struct Move move = generate_move(frontend_state.game_state);
        if (!boardpos_eq(move.from, NULL_BOARDPOS)) {
            log_move(move.from, move.to);
            make_move(frontend_state.game_state, move);
            frontend_state.selected_position = NULL_BOARDPOS;

            if (is_player_checkmated(frontend_state.game_state, WhitePlayer)) {
                frontend_state.winner = player == WhitePlayer ? 0 : 1;
            } else if (is_stalemate(frontend_state.game_state)) {
                frontend_state.winner = 2;
            }
        }
    }

    // Human moves
    if (frontend_state.winner == -1 && (frontend_state.game_state->white_to_move || frontend_state.two_player_mode) &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        int x = GetMouseX();
        int y = GetMouseY();

        if (x < BOARD_SIZE && y < BOARD_SIZE) {
            enum Player player_to_move = frontend_state.game_state->white_to_move ? WhitePlayer : BlackPlayer;
            struct BoardPos pos = BoardPos(x / BOARD_SQUARE_SIZE, y / BOARD_SQUARE_SIZE);
            struct Piece piece = get_piece(frontend_state.game_state, pos);

            // Piece selection and moving
            if (piece.type != Empty && piece.player == player_to_move) {
                frontend_state.selected_position = pos;
            } else if (!boardpos_eq(frontend_state.selected_position, NULL_BOARDPOS)) {
                if (frontend_state.debug_allow_illegal_moves ||
                    is_move_legal(frontend_state.game_state, (struct Move){frontend_state.selected_position, pos})) {
                    // Add the move to the move log
                    log_move(frontend_state.selected_position, pos);

                    // Make the move
                    make_move(frontend_state.game_state, (struct Move){frontend_state.selected_position, pos});

                    // BUG: doesnt add to piece list
                    if (frontend_state.debug_copy_on_move) {
                        put_piece(frontend_state.game_state, get_piece(frontend_state.game_state, pos),
                                  frontend_state.selected_position);
                    }

                    frontend_state.selected_position = NULL_BOARDPOS;

                    if (is_player_checkmated(frontend_state.game_state, other_player(player_to_move))) {
                        frontend_state.winner = player_to_move == WhitePlayer ? 0 : 1;
                    } else if (is_stalemate(frontend_state.game_state)) {
                        frontend_state.winner = 2;
                    }
                }
            }
        }
    }

    draw_board(frontend_state.game_state);

    // Draw a circle on the currently selected piece
    if (!boardpos_eq(frontend_state.selected_position, NULL_BOARDPOS)) {
        struct BoardPos pos = frontend_state.selected_position;
        int x = pos.file * BOARD_SQUARE_SIZE;
        int y = pos.rank * BOARD_SQUARE_SIZE;
        DrawCircle(x + BOARD_SQUARE_SIZE / 2, y + BOARD_SQUARE_SIZE / 2, BOARD_SQUARE_SIZE / 6.0f,
                   Fade(DARKBLUE, 0.8f));
    }
}

int main() {
#if defined(CHESS_TEST)
    test_main();
    return EXIT_SUCCESS;
#elif !defined(NDEBUG)
    puts("DEBUG BUILD!");
#endif

    srand(time(NULL));  // seed the prng, used for choosing opening book moves

    // Initialise the window
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "chess");
    SetTargetFPS(15);
    SetWindowMinSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    GuiSetStyle(DEFAULT, TEXT_SIZE, 16);

    // Load required assets and create a new game
    load_textures();
    init_opening_book();
    frontend_new_game();

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(WHITE);
        if (frontend_state.game_state != NULL) game_loop();
        draw_ui();
        EndDrawing();
    }

    // Cleanup
    if (frontend_state.game_state != NULL) {
        deinit_gamestate(frontend_state.game_state);
    }

    if (frontend_state.move_log != NULL) {
        free(frontend_state.move_log);
    }

    deinit_opening_book();
    unload_textures();
    CloseWindow();
    return EXIT_SUCCESS;
}
