#include <assert.h>
#include <chess.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zobrist.h>

// A boardpos signifiying 'no position'
// For example, it is returned by the engines move generator
// as the `from` of a Move struct when there are no legal moves
const struct BoardPos NULL_BOARDPOS = {0xf, 0xf};

// BoardPos constructor
struct BoardPos BoardPos(int8_t file, int8_t rank) { return (struct BoardPos){file, rank}; }

// Check if two BoardPos are equal
bool boardpos_eq(struct BoardPos a, struct BoardPos b) {
    assert(sizeof(struct BoardPos) == 2);
    return memcmp(&a, &b, 2) == 0;
}

// Add two BoardPos, returning NULL_BOARDPOS if the result is outside the board
struct BoardPos boardpos_add(struct BoardPos a, struct BoardPos b) {
    struct BoardPos r = {a.file + b.file, a.rank + b.rank};
    return (r.file > 7 || r.rank > 7 || r.file < 0 || r.rank < 0) ? NULL_BOARDPOS : r;
}

// Piece constructor
struct Piece Piece(enum PieceType type, enum Player player) {
    struct Piece p = {type, player};
    return p;
}

// Put a piece onto the board
// `pos` must be a valid position
void put_piece(struct GameState *state, struct Piece piece, struct BoardPos pos) {
    assert(pos.file >= 0 && pos.file < 8 && pos.rank >= 0 && pos.rank < 8);
    state->board[pos.file][pos.rank] = piece;
}

// Get a piece from the board
// `pos` must be a valid position
struct Piece get_piece(struct GameState *state, struct BoardPos pos) {
    assert(pos.file >= 0 && pos.file < 8 && pos.rank >= 0 && pos.rank < 8);
    return state->board[pos.file][pos.rank];
}

// Returns the enpassant target file for a player, or -1 if there is no target
int get_enpassant_target_file(struct GameState *state, enum Player player) {
    if (player == WhitePlayer) {
        return state->enpassant_target_white;
    } else {
        return state->enpassant_target_black;
    }
}

// Unset an en passant target file for the player
void unset_enpassant_target_file(struct GameState *state, enum Player attacking_player) {
    if (attacking_player == WhitePlayer) {
        state->enpassant_target_white = -1;
    } else {
        state->enpassant_target_black = -1;
    }
}

// Unset the left side castling right for a player
void unset_castlert_left(struct GameState *state, enum Player player) {
    if (player == WhitePlayer)
        state->white_castlert_left = false;
    else
        state->black_castlert_left = false;
}

// Unset the right side castling right for a player
void unset_castlert_right(struct GameState *state, enum Player player) {
    if (player == WhitePlayer)
        state->white_castlert_right = false;
    else
        state->black_castlert_right = false;
}

// Removes all pieces from the board, also removing the stored positions
// of the kings
void clear_board(struct GameState *state) {
    memset(state->board, 0, sizeof(state->board));
    memset(state->piece_list_white, NULL_BOARDPOS.file, sizeof(state->piece_list_white));
    memset(state->piece_list_black, NULL_BOARDPOS.file, sizeof(state->piece_list_black));
    state->white_king = NULL_BOARDPOS;
    state->black_king = NULL_BOARDPOS;
}

// Constructs a new gamestate representing the start of a default chess game
// Return value must be freed via deinit_gamestate()
struct GameState *init_gamestate() {
    struct GameState *state = malloc(sizeof(*state));
    clear_board(state);
    state->white_to_move = true;
    state->enpassant_target_white = -1;
    state->enpassant_target_black = -1;
    state->white_castlert_left = true;
    state->white_castlert_right = true;
    state->black_castlert_left = true;
    state->black_castlert_right = true;
    state->white_king = BoardPos(4, 7);
    state->black_king = BoardPos(4, 0);
    state->white_king_in_check = false;
    state->black_king_in_check = false;
    state->move_count = 0;

    // Order of pieces on the initial chess board, excluding pawns
    static const enum PieceType PIECES_ORDER[8] = {Rook, Knight, Bishop, Queen, King, Bishop, Knight, Rook};

    for (int i = 0; i < 8; i++) {
        // Place pawns
        put_piece(state, Piece(Pawn, BlackPlayer), BoardPos(i, 1));
        put_piece(state, Piece(Pawn, WhitePlayer), BoardPos(i, 6));

        // Place other pieces
        put_piece(state, Piece(PIECES_ORDER[i], BlackPlayer), BoardPos(i, 0));
        put_piece(state, Piece(PIECES_ORDER[i], WhitePlayer), BoardPos(i, 7));

        // Add pieces to piece list
        // Pawns are stored at the end of the list as they will likely be lost first
        state->piece_list_white[i + 8] = BoardPos(i, 6);
        state->piece_list_black[i + 8] = BoardPos(i, 1);
        // Other pieces
        state->piece_list_white[i] = BoardPos(i, 7);
        state->piece_list_black[i] = BoardPos(i, 0);
    }

    state->hash = hash_state(state);
    return state;
}

// Deallocates the GameState
void deinit_gamestate(struct GameState *state) { free(state); }

// Returns a copy of the gamestate, must be deallocated
struct GameState *copy_gamestate(struct GameState *state) {
    struct GameState *new = malloc(sizeof(*state));
    memcpy(new, state, sizeof(*state));
    return new;
}

// Sets the stored king position for a player
void set_king_pos(struct GameState *state, enum Player player, struct BoardPos pos) {
    player == WhitePlayer ? (state->white_king = pos) : (state->black_king = pos);
}

// Gets the stored king position for a player
struct BoardPos get_king_pos(struct GameState *state, enum Player player) {
    return player == WhitePlayer ? state->white_king : state->black_king;
}

// Checks if a player's king is in check
bool is_player_in_check(struct GameState *state, enum Player player) {
    return player == WhitePlayer ? state->white_king_in_check : state->black_king_in_check;
}

// Set a player's king in check status
void set_player_in_check(struct GameState *state, enum Player player, bool in_check) {
    player == WhitePlayer ? (state->white_king_in_check = in_check) : (state->black_king_in_check = in_check);
}

// Returns the other player, i.e. white if `player` is black and black if `player` is white
enum Player other_player(enum Player player) { return player == WhitePlayer ? BlackPlayer : WhitePlayer; }

// Swaps a position in the piece list with another position for a player
void change_piece_list_pos(struct GameState *state, enum Player player, struct BoardPos from, struct BoardPos to) {
    struct BoardPos *piece_list = player == WhitePlayer ? state->piece_list_white : state->piece_list_black;
    for (int i = 0; i < 16; i++) {
        if (boardpos_eq(from, piece_list[i])) {
            piece_list[i] = to;
        }
    }
}

// Prints out the state of the board to stdout, used for debugging
static const char *PIECE_NAMES[7] = {"Empty", "King", "Queen", "Rook", "Bishop", "Knight", "Pawn"};
void print_gamestate(struct GameState *state) {
    printf("GameState {\n");
    for (int file = 0; file < 8; file++) {
        for (int rank = 0; rank < 8; rank++) {
            struct Piece p = get_piece(state, BoardPos(file, rank));
            printf("\t%d, %d : %s %s\n", file, rank, (p.player == WhitePlayer ? "White" : "Black"),
                   PIECE_NAMES[p.type]);
        }
    }
    printf("}\n");
}

// Convert a boardpos to algrebraic notation
// https://en.wikipedia.org/wiki/Algebraic_notation_(chess)
// `buf` must be at least 2 bytes
void boardpos_to_algn(struct BoardPos pos, char *buf) {
    buf[0] = pos.file + 'a';
    buf[1] = 8 - pos.rank + '0';
}

// Convert a piece to its algebraic notation character (returns \0 if the piece
// is a pawn or empty)
char piece_to_algn(struct Piece piece) {
    static const char PIECES[7] = {'\0', 'K', 'Q', 'R', 'B', 'N', '\0'};
    return PIECES[piece.type];
}

// Converts a move to a string (close but not strict algebraic notation, doesnt consider if in
// check etc).
// `buf` must be at least 5 bytes.
// Returns the number of bytes written.
int move_to_str(struct GameState *state, struct BoardPos from, struct BoardPos to, char *buf) {
    struct Piece from_piece = get_piece(state, from);

    // Castling moves have a special format.
    if (from_piece.type == King && abs(from.file - to.file) == 2) {
        if (to.file == 2) {
            // Queenside castle.
            memcpy(buf, "0-0-0", 5);
            return 5;
        } else {
            // Kingside castle.
            memcpy(buf, "0-0", 3);
            return 3;
        }

    } else {
        // Add the from piece's character first.
        buf[0] = piece_to_algn(from_piece);

        // The pawn does not have a character in algebraic notation, the pawn's file is used instead.
        // So in the case of a pawn \0 is added at buf[0].
        char *to_buf = buf[0] == '\0' ? buf : buf + 1;
        struct Piece to_piece = get_piece(state, to);

        // Capture moves have an 'x' added.
        if (to_piece.type != Empty) {
            // to_buf == buf when the from piece is a pawn.
            if (to_buf == buf) {
                boardpos_to_algn(from, to_buf);
                to_buf++;
            }

            *to_buf++ = 'x';
        }

        // Add the destination square.
        boardpos_to_algn(to, to_buf);

        // The number of bytes written is the difference between to_buf and buf (which depends on if the move was a
        // capture and if it was a pawn move) + the two bytes written for the destination square.
        return to_buf - buf + 2;
    }
}

// Moves the piece at `from` to `to`. Simply replaces the piece at `to` with the
// piece at `from`, then puts Empty at `from`. Does not update other state (e.g.
// en passant), handle castling or check legality.
void move_piece(struct GameState *state, struct BoardPos from, struct BoardPos to) {
    put_piece(state, get_piece(state, from), to);
    put_piece(state, Piece(Empty, WhitePlayer), from);
}