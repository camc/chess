#include <config.h>
#include <engine.h>
#include <fen.h>
#include <frontend_state.h>
#include <raygui.h>
#include <stdio.h>
#include <string.h>

// Draws the UI onto the window
// Must be called within BeginDrawing/EndDrawing
void draw_ui() {
    static const int LABEL_WIDTH = (int)((3.0f / 7.0f) * (WINDOW_WIDTH - BOARD_SIZE));
    static const int BUTTON_WIDTH = (int)((2.0f / 7.0f) * (WINDOW_WIDTH - BOARD_SIZE));

    // Draw borders around the grouped buttons & labels
    DrawRectangleLinesEx((Rectangle){BOARD_SIZE, 0, WINDOW_WIDTH - BOARD_SIZE, 30}, 2, GRAY);
    DrawRectangleLinesEx((Rectangle){BOARD_SIZE, 30, WINDOW_WIDTH - BOARD_SIZE, 30}, 2, GRAY);

    // Draw the game type labels
    GuiLabel((Rectangle){BOARD_SIZE, 0, LABEL_WIDTH, 30}, " Two player mode");
    GuiLabel((Rectangle){BOARD_SIZE, 30, LABEL_WIDTH, 30}, " Computer mode");

    // Draw the new two player game button
    if (GuiButton((Rectangle){BOARD_SIZE + LABEL_WIDTH, 0, BUTTON_WIDTH, 30}, "New Game")) {
        frontend_state.two_player_mode = true;
        frontend_new_game();
    }

    // Draw the load two player game button
    if (GuiButton((Rectangle){BOARD_SIZE + LABEL_WIDTH + BUTTON_WIDTH, 0, BUTTON_WIDTH, 30}, "Load Game")) {
        const char *fen = GetClipboardText();
        if (fen == NULL || !frontend_new_game_from_fen(fen)) {
            // If the provided FEN is invalid display a message notifying the user
            // and start a default game
            frontend_state.message_box =
                "An invalid game was provided.\nMake sure a valid FEN string is copied to the clipboard.";
            frontend_new_game();
        }

        frontend_state.two_player_mode = true;
    }

    // Draw the save game button, only if there is a game ongoing
    if (frontend_state.game_state != NULL &&
        GuiButton((Rectangle){BOARD_SIZE, WINDOW_HEIGHT - 30, WINDOW_WIDTH - BOARD_SIZE, 30}, "Save game")) {
        char fen[90];
        gamestate_to_fen(frontend_state.game_state, fen);
        SetClipboardText(fen);
        frontend_state.message_box =
            "The game was saved to the clipboard.\nYou can paste it where you like so that you can reload it later.";
    }

    // Draw the new computer game button
    if (GuiButton((Rectangle){BOARD_SIZE + LABEL_WIDTH, 30, BUTTON_WIDTH, 30}, "New Game")) {
        frontend_state.two_player_mode = false;
        frontend_new_game();
    }

    // Draw the load computer game button
    if (GuiButton((Rectangle){BOARD_SIZE + LABEL_WIDTH + BUTTON_WIDTH, 30, BUTTON_WIDTH, 30}, "Load Game")) {
        const char *fen = GetClipboardText();
        if (fen == NULL || !frontend_new_game_from_fen(fen)) {
            // If the provided FEN is invalid display a message notifying the user
            // and start a default game
            frontend_state.message_box =
                "An invalid game was provided.\nMake sure a valid game string is copied to the clipboard.";
            frontend_new_game();
        }

        frontend_state.two_player_mode = false;
    }

    // Draw the move log text
    if (frontend_state.move_log_idx != 0) {
        GuiTextBoxMulti((Rectangle){BOARD_SIZE, BOARD_SQUARE_SIZE * 2, WINDOW_WIDTH - BOARD_SIZE, 0},
                        frontend_state.move_log, 2, false);
    }

    // Draw the game over winner/draw text or the winner prediction
    if (frontend_state.winner != -1) {
        char label_s[23] = "Game over! White wins";
        if (frontend_state.winner == 1) {
            memcpy(label_s + 11, "Black", 5);
        } else if (frontend_state.winner == 2) {
            memcpy(label_s + 11, "Draw", 5);
        }

        GuiLabel((Rectangle){BOARD_SIZE + 5, 60, WINDOW_WIDTH - BOARD_SIZE, 30}, label_s);
    } else {
        int value = position_value(frontend_state.game_state);

        // Limit certainty to 95% since this is a really bad way of predicting...
        int certainty = 100.0f * (float)abs(value) / (float)ROUGH_MAX_POSITION_VALUE;
        certainty = certainty > 95 ? 95 : certainty;

        const char *winner = "Unknown";
        if (value > 0 && certainty != 0) {
            winner = "White";
        } else if (value < 0 && certainty != 0) {
            winner = "Black";
        }

        // "Predicted winner: xxxxxxx (XX% certainty)"
        char label[51] = {0};
        snprintf(label, 51, "Predicted winner: %s (%d%% certainty)", winner, certainty);

        // Don't bother saying 0% certainty of unknown winner
        if (value == 0 || certainty == 0) {
            label[25] = '\0';
        }

        GuiLabel((Rectangle){BOARD_SIZE + 5, 60, WINDOW_WIDTH - BOARD_SIZE, 30}, label);
    }

    // Draw a message box if there is a message
    if (frontend_state.message_box != NULL) {
        if (GuiMessageBox((Rectangle){WINDOW_WIDTH / 5, WINDOW_HEIGHT / 4, (WINDOW_WIDTH * 3) / 5, WINDOW_HEIGHT / 2},
                          "Info", frontend_state.message_box, "Close") != -1) {
            // Remove the message if the box was closed
            frontend_state.message_box = NULL;
        }
    }
}
