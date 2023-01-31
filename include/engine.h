#ifndef ENGINE_H
#define ENGINE_H

#include <chess.h>
#include <stdbool.h>
#include <threadpool.h>
#include <time.h>

// The maximum number of legal moves a single piece can have
#define PIECE_LEGAL_MOVES_MAX 27

// The rough maximum value a position can have as calulated by position_value
// Used for estimating who will win
#define ROUGH_MAX_POSITION_VALUE 4000

bool is_piece_attacked(struct GameState *state, struct BoardPos attackee_pos, enum Player attacker);
bool is_move_legal(struct GameState *state, struct Move move);
void make_move(struct GameState *state, struct Move move, bool calculate_hash);
bool is_player_checkmated(struct GameState *state, enum Player player);
void generate_move(struct GameState *state, struct ThreadPool *pool, time_t start_time);
bool is_stalemate(struct GameState *state);
int position_value(struct GameState *state);

#endif /* ENGINE_H */
