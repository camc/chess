#include <config.h>
#include <draw.h>
#include <raylib.h>
#include <stdio.h>

// Stores the textures used to draw chess pieces
// Must be initialised before use by calling load_textures()
static Texture2D white_piece_textures[6] = {0};
static Texture2D black_piece_textures[6] = {0};

// Load the piece textures from disk (stored in $PWD/res/) into VRAM
// Texture info stored in a static array, resizing the images if needed
// Textures must be unloaded from VRAM with unload_textures()
void load_textures() {
    for (int i = 0; i < 6; i += 1) {
        char filename[18];
        sprintf(filename, "res/piece_%d_w.png", i);
        Image piece_image = LoadImage(filename);
        ImageResize(&piece_image, BOARD_SQUARE_SIZE, BOARD_SQUARE_SIZE);
        white_piece_textures[i] = LoadTextureFromImage(piece_image);
        UnloadImage(piece_image);

        sprintf(filename, "res/piece_%d_b.png", i);
        piece_image = LoadImage(filename);
        ImageResize(&piece_image, BOARD_SQUARE_SIZE, BOARD_SQUARE_SIZE);
        black_piece_textures[i] = LoadTextureFromImage(piece_image);
        UnloadImage(piece_image);
    }
}

// Unload textures which were loaded with `load_textures` from VRAM
void unload_textures() {
    for (int i = 0; i < 6; i++) {
        UnloadTexture(white_piece_textures[i]);
        UnloadTexture(black_piece_textures[i]);
    }
}

// Draws a chess piece at a specified square position
// Piece textures must be loaded prior to call via load_textures()
// Should be called within BeginDrawing/EndDrawing
static void draw_piece(struct Piece piece, struct BoardPos pos) {
    if (piece.type == Empty) return;
    Texture2D texture = (piece.player == WhitePlayer ? white_piece_textures : black_piece_textures)[piece.type - 1];
    DrawTexture(texture, BOARD_SQUARE_SIZE * pos.file, BOARD_SQUARE_SIZE * pos.rank, WHITE);
}

// Draws a chess board from a GameState, including the pieces and the board background
// Should be called within BeginDrawing/EndDrawing
void draw_board(struct GameState *state) {
    for (int file = 0; file < 8; file++) {
        for (int rank = 0; rank < 8; rank++) {
            // Draw the square background
            Color square_colour = GetColor((file + rank) % 2 == 0 ? LIGHT_SQUARE_COLOUR : DARK_SQUARE_COLOUR);
            DrawRectangle(file * BOARD_SQUARE_SIZE, rank * BOARD_SQUARE_SIZE, BOARD_SQUARE_SIZE, BOARD_SQUARE_SIZE,
                          square_colour);

            // Draw the piece
            draw_piece(get_piece(state, BoardPos(file, rank)), BoardPos(file, rank));
        }
    }
}
