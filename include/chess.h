#ifndef CHESS_H
#define CHESS_H

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t ZobristHash;

// Each chess piece type is assigned a unique value which is used in the board to identify pieces
enum PieceType {
    Empty = 0,
    King = 1,
    Queen = 2,
    Rook = 3,
    Bishop = 4,
    Knight = 5,
    Pawn = 6,
};

enum Player { WhitePlayer = 0, BlackPlayer = 1 };

// Each piece on the board has a type and a player
struct Piece {
    enum PieceType type;
    enum Player player;
};

// Stores representation of a position on the board.
// {file 0, rank 0} is the top left of the board (from whites POV)
// and {file 7, rank 7} is the bottom right (from whites POV)
// The file and rank are signed so that the BoardPos can also be used to
// store directions of movement, e.g. {file - 1, rank 0} would be moving in
// the 'east' direction (from whites POV)
__attribute__((packed)) struct BoardPos {
    int8_t file;
    int8_t rank;
};

// Structure used to store a move (actually a ply), used by the engine
// when generating moves etc.
struct Move {
    struct BoardPos from;
    struct BoardPos to;
};

// The GameState struct stores all information about an ongoing game that is used
// by the engine.
// It can be initialised to the starting state using init_gamestate()
struct GameState {
    struct Piece board[8][8];              // column/file major 2d array
    bool white_to_move;                    // set to true if it is whites move
    int8_t enpassant_target_white;         // En passant target file for white (-1 if no target)
    int8_t enpassant_target_black;         // En passant target file for black (-1 if no target)
    bool white_castlert_left;              // Castling rights, where left is the rook at file=0
    bool white_castlert_right;             //
    bool black_castlert_left;              //
    bool black_castlert_right;             //
    struct BoardPos white_king;            // White king position
    struct BoardPos black_king;            // Black king position
    bool white_king_in_check;              // True if the white king is currently in check
    bool black_king_in_check;              // True if the black king is currently in check
    int move_count;                        // Number of moves played (actually number of ply)
    struct BoardPos piece_list_white[16];  // A list of the positions of all white pieces
    struct BoardPos piece_list_black[16];  // A list of the positions of all black pieces
    ZobristHash hash;                      // The zobrist hash of the state
};

extern const struct BoardPos NULL_BOARDPOS;

struct BoardPos BoardPos(int8_t file, int8_t rank);
bool boardpos_eq(struct BoardPos a, struct BoardPos b);
struct BoardPos boardpos_add(struct BoardPos a, struct BoardPos b);
struct Piece Piece(enum PieceType, enum Player);
void put_piece(struct GameState *state, struct Piece piece, struct BoardPos pos);
struct Piece get_piece(struct GameState *state, struct BoardPos pos);
struct GameState *init_gamestate();
void deinit_gamestate(struct GameState *state);
int get_enpassant_target_file(struct GameState *state, enum Player player);
void unset_enpassant_target_file(struct GameState *state, enum Player attacking_player);
void unset_castlert_left(struct GameState *state, enum Player player);
void unset_castlert_right(struct GameState *state, enum Player player);
void boardpos_to_algn(struct BoardPos pos, char *buf);
int move_to_str(struct GameState *state, struct BoardPos from, struct BoardPos to, char *buf);
struct GameState *copy_gamestate(struct GameState *state);
void set_king_pos(struct GameState *state, enum Player player, struct BoardPos pos);
struct BoardPos get_king_pos(struct GameState *state, enum Player player);
bool is_player_in_check(struct GameState *state, enum Player player);
enum Player other_player(enum Player player);
void change_piece_list_pos(struct GameState *state, enum Player player, struct BoardPos from, struct BoardPos to);
void print_gamestate(struct GameState *state);
void clear_board(struct GameState *state);
void set_player_in_check(struct GameState *state, enum Player player, bool in_check);
void move_piece(struct GameState *state, struct BoardPos from, struct BoardPos to);

#endif /* CHESS_H */
