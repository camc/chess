#include <assert.h>
#include <config.h>
#include <engine.h>
#include <limits.h>
#include <math.h>
#include <openings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threadpool.h>
#include <time.h>
#include <tptable.h>
#include <util.h>
#include <zobrist.h>

// clang-format off
// A list of all move directions white piece can make in (x,y), indexed by the (piece id - 1)
// where {0xf, 0xf} is the null boardpos (no direction)
// The magnitude is also stored e.g. for knight 2 is used to represent the long part of the  'L-shape'
static const struct BoardPos PIECE_MOVE_DIRECTIONS[6][8] = {
    {{0, 1}, {1, 1}, {1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {1, -1}}, // Queen
    {{0, 1}, {1, 1}, {1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, {1, -1}}, // King
    {{0, 1}, {0, -1}, {-1, 0}, {1, 0}, {0xf, 0xf}, {0xf, 0xf}, {0xf, 0xf}, {0xf, 0xf}}, // Rook
    {{1, 1}, {-1, -1}, {1, -1}, {-1, 1}, {0xf, 0xf}, {0xf, 0xf}, {0xf, 0xf}, {0xf, 0xf}}, // Bishop
    {{2, 1}, {2, -1}, {-2, 1}, {-2, -1}, {1, 2}, {-1, 2}, {1, -2}, {-1, -2}}, // Knight
    {{1, -1}, {-1, -1}, {0, -1}, {0, -2}, {0xf, 0xf}, {0xf, 0xf}, {0xf, 0xf}, {0xf, 0xf}} // Pawn
};
// clang-format on

// The starting positions for the white and black rooks
const struct BoardPos ROOK_STARTING_POSITIONS_LEFT[2] = {{0, 7}, {0, 0}};
const struct BoardPos ROOK_STARTING_POSITIONS_RIGHT[2] = {{7, 7}, {7, 0}};

// The starting positions for the white and black kings
const struct BoardPos KING_STARTING_POSITIONS[2] = {{4, 7}, {4, 0}};

// Writes an array of the legal move destinations the piece at `initial` can make to `moves_dest`.
// `moves_dest` must be an array of length at least `PIECE_LEGAL_MOVES_MAX` (defined in engine.h).
// Returns the number of legal moves.
static unsigned int legal_moves_from_pos(struct GameState *state, struct BoardPos initial,
                                         struct BoardPos *moves_dest) {
    struct Piece piece = get_piece(state, initial);
    unsigned int moves_idx = 0;

    // Each piece has different patterns which they are allowed to move in.
    // Every move possibility will be checked for the piece type to get all the legal moves.
    switch (piece.type) {
        case King:
            for (int i = 0; i < 8; i++) {
                // Check each possible move direction to see if it is legal.
                struct BoardPos direction = PIECE_MOVE_DIRECTIONS[piece.type - 1][i];
                struct BoardPos check = boardpos_add(initial, direction);
                if (!boardpos_eq(check, NULL_BOARDPOS)) {
                    if (is_move_legal(state, (struct Move){initial, check})) {
                        moves_dest[moves_idx++] = check;
                    }
                }
            }

            // Check each possible castling move if the king is at its starting position.
            if (boardpos_eq(initial, KING_STARTING_POSITIONS[piece.player == WhitePlayer ? 0 : 1])) {
                static const struct BoardPos castling_directions[2] = {{2, 0}, {-2, 0}};
                for (int i = 0; i < 2; i++) {
                    struct BoardPos check = boardpos_add(initial, castling_directions[i]);
                    if (is_move_legal(state, (struct Move){initial, check})) {
                        moves_dest[moves_idx++] = check;
                    }
                }
            }

            break;

        case Queen:
        case Rook:
        case Bishop:
            for (int i = 0; i < 8 && !boardpos_eq(PIECE_MOVE_DIRECTIONS[piece.type - 1][i], NULL_BOARDPOS); i++) {
                // Check each possible move direction for legality.
                struct BoardPos check = boardpos_add(initial, PIECE_MOVE_DIRECTIONS[piece.type - 1][i]);

                // The queen, rook and bishop can move multiple times in the same direction.
                while (!boardpos_eq(check, NULL_BOARDPOS)) {
                    if (is_move_legal(state, (struct Move){initial, check})) {
                        moves_dest[moves_idx++] = check;
                    }

                    check = boardpos_add(check, PIECE_MOVE_DIRECTIONS[piece.type - 1][i]);
                }
            }
            break;

        case Knight:
            for (int i = 0; i < 8 && !boardpos_eq(PIECE_MOVE_DIRECTIONS[piece.type - 1][i], NULL_BOARDPOS); i++) {
                // Check each possible move direction for legality.
                struct BoardPos direction = PIECE_MOVE_DIRECTIONS[piece.type - 1][i];
                struct BoardPos check = boardpos_add(initial, direction);
                if (!boardpos_eq(check, NULL_BOARDPOS)) {
                    if (is_move_legal(state, (struct Move){initial, check})) {
                        moves_dest[moves_idx++] = check;
                    }
                }
            }
            break;

        case Pawn:
            for (int i = 0; i < 8 && !boardpos_eq(PIECE_MOVE_DIRECTIONS[piece.type - 1][i], NULL_BOARDPOS); i++) {
                // Check each possible move direction for legality.
                struct BoardPos direction = PIECE_MOVE_DIRECTIONS[piece.type - 1][i];

                // If it is black moving the pawns move in the opposite direction.
                if (piece.player == BlackPlayer) {
                    direction.file *= -1;
                    direction.rank *= -1;
                }

                struct BoardPos check = boardpos_add(initial, direction);
                if (!boardpos_eq(check, NULL_BOARDPOS)) {
                    if (is_move_legal(state, (struct Move){initial, check})) {
                        moves_dest[moves_idx++] = check;
                    }
                }
            }
            break;

        default:
            return 0;
    }

    return moves_idx;
}

// Writes a list of all the legal moves for a player, ordered using a heuristic to place the better moves first.
// The list must be deallocated.
// Returns the number of moves in the list.
static unsigned int all_legal_moves_ordered(struct GameState *state, enum Player player, struct Move **moves_out) {
    // Captures and other moves will be collected separately, as captures are likely to be better moves.
    int moves_size = 50;
    int moves_idx = 0;
    int captures_size = 9;
    int captures_idx = 0;
    struct Move *moves = malloc(sizeof(struct Move) * 50);
    struct Move *captures = malloc(sizeof(struct Move) * 9);

    // If there is a principal variation stored in the transposition table for this position, place that move first as
    // it is known to be the best.
    struct TranspositionEntry tp_entry = tptable_get(state->hash);
    bool has_pvn = tp_entry.depth != 0 && !boardpos_eq(tp_entry.best_move.from, NULL_BOARDPOS);
    bool waiting_for_pvn = has_pvn;

    // Get all legal moves for each piece.
    struct BoardPos *piece_list = player == WhitePlayer ? state->piece_list_white : state->piece_list_black;
    for (int i = 0; i < 16; i++) {
        struct BoardPos from = piece_list[i];
        if (boardpos_eq(from, NULL_BOARDPOS)) continue;
        struct Piece from_piece = get_piece(state, from);

        // If there is insufficent space in the arrays extend them.
        if (moves_size - moves_idx < 27) {
            moves = realloc(moves, sizeof(struct Move) * moves_size * 2);
            moves_size *= 2;
        }

        if (captures_size - captures_idx < 9) {
            captures = realloc(captures, sizeof(struct Move) * captures_size * 2);
            captures_size *= 2;
        }

        // Get all the legal moves for this piece.
        struct BoardPos legal_moves[PIECE_LEGAL_MOVES_MAX];
        int move_count = legal_moves_from_pos(state, from, legal_moves);

        // Add each move to either the captures array or the other moves list.
        for (int m = 0; m < move_count; m++) {
            // If this move is the principal variation then skip, as the principal variation will be added to the start.
            if (waiting_for_pvn && boardpos_eq(tp_entry.best_move.from, from) &&
                boardpos_eq(tp_entry.best_move.to, legal_moves[m])) {
                waiting_for_pvn = false;
                continue;
            }

            // Check if the move is a capture.
            struct Piece to_piece = get_piece(state, legal_moves[m]);
            if ((to_piece.type != Empty && to_piece.player != player) ||
                (from_piece.type == Pawn && from.file != legal_moves[m].file)) {
                captures[captures_idx++] = (struct Move){from, legal_moves[m]};
            } else {
                moves[moves_idx++] = (struct Move){from, legal_moves[m]};
            }
        }
    }

    size_t move_count = moves_idx + captures_idx + has_pvn;

    // Combine the captures and moves into one array, placing the captures first.
    struct Move *combined_moves = malloc(sizeof(struct Move) * move_count);
    int combined_idx = 0;

    // If there is a principal variation place that first.
    if (has_pvn) {
        combined_moves[combined_idx++] = tp_entry.best_move;
    }

    // Copy the captures first.
    memcpy(&combined_moves[combined_idx], captures, captures_idx * sizeof(struct Move));
    free(captures);
    combined_idx += captures_idx;

    // Followed by the other moves.
    memcpy(&combined_moves[combined_idx], moves, moves_idx * sizeof(struct Move));
    free(moves);
    combined_idx += moves_idx;

    *moves_out = combined_moves;
    return move_count;
}

// Returns a value represeting how good a chess position is for white.
// Checkmate & stalemate are not considered - the function assumes the game is ongoing.
// Positive values indicate that the position is better for white, negative values indicate that the position is better
// for black.
int position_value(struct GameState *state) {
    int value = 0;

    // Being in check is bad, the enemy being in check is good.
    if (is_player_in_check(state, WhitePlayer)) {
        value -= 30;
    } else if (is_player_in_check(state, BlackPlayer)) {
        value += 30;
    }

    // Add the values for each piece on the board.
    // Each piece type has a value associated with it, e.g. the value of a queen is greater than that of a pawn as the
    // queen is much more powerful.
    // The values for each piece type are stored in an array indexed by the (piece_type - 1).
    static const int PIECE_VALUES[6] = {20000, 900, 500, 330, 320, 100};
    for (int i = 0; i < 16; i++) {
        // Add the sum of all the piece values for each white piece on the board.
        if (!boardpos_eq(state->piece_list_white[i], NULL_BOARDPOS)) {
            struct Piece piece = get_piece(state, state->piece_list_white[i]);
            value += PIECE_VALUES[piece.type - 1];
        }

        // Subtract the sum of all the piece values for each black piece on the board.
        if (!boardpos_eq(state->piece_list_black[i], NULL_BOARDPOS)) {
            struct Piece piece = get_piece(state, state->piece_list_black[i]);
            value -= PIECE_VALUES[piece.type - 1];
        }
    }

    // Add value if white has castling rights, remove value if black has castling rights.
    value += state->white_castlert_left + state->white_castlert_right;
    value -= state->black_castlert_left + state->black_castlert_right;

    // The more 'safe' the king of a player is the less likely they are to be checkmated.
    // Safety is measured by the number of friendly pieces each player has in the adjacent squares of their kings.
    for (int i = 0; i < 8; i++) {
        struct BoardPos direction = PIECE_MOVE_DIRECTIONS[King - 1][i];
        struct BoardPos check_friendly = boardpos_add(state->white_king, direction);
        struct BoardPos check_enemy = boardpos_add(state->black_king, direction);
        if (!boardpos_eq(check_friendly, NULL_BOARDPOS)) {
            struct Piece piece = get_piece(state, check_friendly);
            // If there is a white piece adjacent to the white king then add value.
            if (piece.type != Empty && piece.player == WhitePlayer) {
                value += 10;
            }
        }

        if (!boardpos_eq(check_enemy, NULL_BOARDPOS)) {
            struct Piece piece = get_piece(state, check_enemy);
            // If there is a black piece adjacent to the black king then subtract value.
            if (piece.type != Empty && piece.player == BlackPlayer) {
                value -= 10;
            }
        }
    }

    // Controlling the centre of the board is advantageous in chess (due to better mobilty etc.).
    // Each of the central squares are checked for the presence of each player's pieces.
    for (int file = 2; file <= 5; file++) {
        for (int rank = 2; rank <= 5; rank++) {
            struct Piece piece = get_piece(state, BoardPos(file, rank));
            if (piece.type == Empty) continue;

            // Add value if white is in the centre, subtract value if black is in the centre.
            // More value is added/subtracted for squares which are more central.
            if (file == 2 || file == 5 || rank == 2 || rank == 5) {
                value += piece.player == WhitePlayer ? 2 : -2;
            } else {
                value += piece.player == WhitePlayer ? 5 : -5;
            }
        }
    }

    return value;
}

// Evaluates the current position, returning a value representing how good the position is for the player to move.
// Recursively calls itself, decreasing `depth` each time. When `depth` = 0 the function returns the heuristic value of
// the postition by calling position_value.
// Alpha-beta pruning is used to improve performance by 'pruning' branches in the game tree to avoid unneeded
// computation.
static int negamax(struct GameState *state, int alpha, int beta, int depth, time_t start_time) {
    enum Player player = state->white_to_move ? WhitePlayer : BlackPlayer;

    // The alpha value at the start of this node evaluation is stored so it can be compared the the evaluation value
    // later to detect if it failed low.
    int start_alpha = alpha;

    // Check the transposition table if we have already evaluated the position at a greater or equal depth then we would
    // be evaluating it now. So that the value can be reused, improving performance.
    struct TranspositionEntry tp_entry = tptable_get(state->hash);
    if (tp_entry.depth != 0 && tp_entry.depth >= depth) {
        if (tp_entry.type == EntryTypeExact) {
            // We have the exact value of this position, return it.
            return tp_entry.value;
        } else if (tp_entry.type == EntryTypeLower) {
            // We have a lower bound on the value of this postition, assign it to alpha so that more cutoffs can occur.
            // Since we have a lower bound, we can be sure that alpha will be the lower bound or more, as alpha is the
            // best value the maximising player has.
            alpha = MAX(alpha, tp_entry.value);
        } else if (tp_entry.type == EntryTypeUpper) {
            // We have an upper bound on the value of this position, assign in to beta so that more cutoffs can occur.
            // Since we have an upper bound, we can be sure that beta will be the upper bound or less, as beta is the
            // best value the minimising player has.
            beta = MIN(beta, tp_entry.value);
        }

        // If the best value the maximising player has is better than the best value the minimising player has then a
        // beta-cutoff occurs, there is no point evaluating this branch as it will never be better for the minimising
        // player than a previously evaluated branch, so the minimising player will choose the previously evaluated
        // branch regardless and the value of this node does not matter.
        if (alpha >= beta) {
            return tp_entry.value;
        }
    }

    // If the game is over return now, there are no legal moves.
    if (is_player_checkmated(state, player)) {
        // Player loses
        return -1000000;
    } else if (is_player_checkmated(state, other_player(player))) {
        // Player wins
        return 1000000;
    } else if (is_stalemate(state)) {
        // If it is stalemate then return 0, a draw so not good for either player.
        return 0;
    }

    // Return the position value if we have no more depth to search.
    if (depth == 0) return position_value(state) * (player == WhitePlayer ? 1 : -1);

    // If the maximum amount of time that can be spent on move generation has elapsed then return now.
    if (time(NULL) - start_time >= MAX_MOVEGEN_SEARCH_TIME) return INT_MIN;

    // Setup the transposition table entry, to be added at the end of the evaluation.
    if (tp_entry.depth == 0) {
        tp_entry.best_move = (struct Move){NULL_BOARDPOS, NULL_BOARDPOS};
    }

    tp_entry.hash = state->hash;
    tp_entry.depth = depth;

    // Store the best value found so it can be returned.
    int best_value = INT_MIN;

    // Get a list of every legal move from this position for the player, and order them using a heuristic so that better
    // moves are ideally first. Alpga-beta pruning performs better if the better moves are first as more beta cutoffs
    // can occur.
    struct Move *legal_moves;
    unsigned int move_count = all_legal_moves_ordered(state, player, &legal_moves);

    // Evaluate each of the moves to find the move with the best value.
    for (unsigned int i = 0; i < move_count; i++) {
        struct Move move = legal_moves[i];

        // A copy of the GameState is created so that the move can be made on it temporarily while it is being
        // evaluated.
        struct GameState *state_copy = copy_gamestate(state);
        make_move(state_copy, move, true);

        // Negamax is recursively called to evaluate the position after the move has been made.
        // The properties `min(a, b) === -max(-a, -b)` and `max(a, b) === -min(-a, -b)` are used to allow the same
        // function to be used for both players (the 'maximising' and 'minimising' players). In the lower nodes alpha,
        // the best value the maximising player is negated to become beta, the best value the minimising player has and
        // vice versa.
        // In each call of negamax the player to move changes, so alpha, beta and the return values are negated.
        int value = negamax(state_copy, -beta, -alpha, depth - 1, start_time);

        // The position has been evaluated and the temporary copy is no longer needed.
        free(state_copy);

        // INT_MIN is returned when the move generation time limit is reached. The value we be bubbled up so it is
        // detected by negamax_from_root.
        if (value == INT_MIN) {
            free(legal_moves);
            return INT_MIN;
        }

        // The value returned by negamax is relative to the player to move (greater values are better for the player to
        // move), so the value is negated so that it is relative to the opposite player, since the value which will be
        // returned in this call represents the value of the parent of the node which is being negated, which is the
        // node before the above move was made, so the opposite player is to move. The value is not immediately negated
        // as that would cause undefined behavior due to signed integer overflow if the value is INT_MIN (on machines
        // which use two's complement |INT_MIN| > |INT_MAX|).
        value = -value;

        // Keep track of the best value we have found.
        if (value > best_value) {
            best_value = value;
            tp_entry.best_move = move;
            // In negamax we act as the maximising player, so update alpha if we have a new max.
            if (value > alpha) {
                alpha = value;
            }
        }

        // If alpha >= beta then a beta cutoff can occur. The best value the 'maximising' player has is better than the
        // best value the best value the 'minimising' player has, so there is no point continuing to evaluate this
        // position as the 'minimising' player will always choose the other branch which we know to be lower than the
        // minimum value this node can be.
        if (alpha >= beta) {
            break;
        }
    }

    free(legal_moves);

    // If best_value is still INT_MIN that means no legal moves were found. But if there are no legal moves that means
    // it is either checkmate or stalemate, which should have been detected earlier in the function.
    // So if it is still INT_MIN then there is a logic error, so assert that it is not INT_MIN to ensure that if it
    // happens it can be detected and fixed. This will not have a performance impact as assertions are only enabled in
    // debug builds.
    assert(best_value != INT_MIN);

    tp_entry.value = best_value;

    // Set the entry type
    if (best_value <= start_alpha) {
        // The value is an upper bound as it failed low, none of the moves
        // searched were greater than the starting alpha.
        tp_entry.type = EntryTypeUpper;
    } else if (best_value >= beta) {
        // The value is a lower bound as it failed high, a move searched was
        // greater than beta (a beta cutoff occured).
        // When a beta cutoff occurs we have stopped in the middle of maximising, so it is a lower bound since
        // maximising can only increase the value.
        tp_entry.type = EntryTypeLower;
    } else {
        // The value improved for both the minimising and maximising player,
        // so there is a principal variation node and an exact value can be returned.
        tp_entry.type = EntryTypeExact;
    }

    // Add an entry to the transposition table so that the results of this evaluation can be reused later.
    tptable_put(tp_entry);

    return best_value;
}

// Used for testing
// https://www.chessprogramming.org/Perft
// NOTE only queen promotions are counted, so results will be lower if there are promotion moves
// static uint64_t perft(struct GameState *state, int depth, enum Player player) {
//     if (depth == 0) return 1ULL;

//     uint64_t nodes = 0;
//     struct Move *legal_moves = all_legal_moves_ordered(state, player);
//     for (int i = 0; !boardpos_eq(legal_moves[i].from, NULL_BOARDPOS); i++) {
//         struct Move move = legal_moves[i];

//         struct GameState *state_copy = copy_gamestate(state);
//         make_move(state_copy, move.from, move.to);
//         nodes += perft(state_copy, depth - 1, other_player(player));
//         free(state_copy);
//     }

//     free(legal_moves);
//     return nodes;
// }

// Returns the best move for the player to move from the current position. Calls negamax to evaluate each possible move,
// and returns the best. Returns a Move with NULL_BOARDPOS as the `from` if the time limit was reached or there are no
// legal moves.
static void negamax_from_root(struct GameState *state, int depth, time_t start_time) {
    // Inititalise alpha and beta to the starting values.
    // In the alpha-beta pruning algorithm alpha is used to store the best value the maximising player has so far and
    // beta is used to store the best value the minimising player has so far.
    // So, alpha is initialised to the worst possible value for the maximising player (actually INT_MIN + 1 to avoid
    // signed integer overflow on negation) and beta is initialised to the worst possible value for the minimising
    // player.
    int alpha = INT_MIN + 1;
    const int beta = INT_MAX;

    enum Player player = state->white_to_move ? WhitePlayer : BlackPlayer;

    // The best move found will be stored so it can be returned.
    struct Move best_move = (struct Move){NULL_BOARDPOS, NULL_BOARDPOS};

    // The best value found will be stored so that moves can be compared.
    int best_value = INT_MIN;

    // Get a list of every legal move from this position for the player, and order them using a heuristic so that better
    // moves are ideally first. Alpha-beta pruning performs better if the better moves are first as more beta cutoffs
    // can occur.
    struct Move *legal_moves;
    unsigned int move_count = all_legal_moves_ordered(state, player, &legal_moves);

    // Every legal move is evaluated and compared to find the move with the highest value, the best move for the player.
    for (unsigned int i = 0; i < move_count; i++) {
        struct Move move = legal_moves[i];

        // A copy of the state is created so the current state is not affected by the move.
        struct GameState *state_copy = copy_gamestate(state);

        // Make the move
        make_move(state_copy, move, true);

        // Negamax is called to evaluate the position after the move has been made.
        // The properties `min(a, b) === -max(-a, -b)` and `max(a, b) === -min(-a, -b)` are used to allow the same
        // function to be used for both players (the 'maximising' and 'minimising' players). In the lower nodes alpha,
        // the best value the maximising player is negated to become beta, the best value the minimising player has and
        // vice versa.
        // In each call of negamax the player to move changes, so alpha, beta and the return values are negated.
        int value = negamax(state_copy, -beta, -alpha, depth - 1, start_time);
        free(state_copy);

        // INT_MIN is returned when the time limit is reached.
        if (value == INT_MIN) {
            free(legal_moves);
            return;
        }

        // The value returned by negamax is relative to the player to move (greater values are better for the player to
        // move), so the value is negated so that it is relative to the opposite player, since the value which will be
        // returned in this call represents the value of the parent of the node which is being negated, which is the
        // node before the above move was made, so the opposite player is to move. The value is not immediately negated
        // as that would cause undefined behavior due to signed integer overflow if the value is INT_MIN (on machines
        // which use two's complement |INT_MIN| > |INT_MAX|).
        value = -value;

        // Update the best value and alpha if needed.
        // Alpha is the best value the maximising player has so far.
        if (value > best_value) {
            best_value = value;
            best_move = move;
            if (value > alpha) {
                alpha = value;
            }
        }
    }

    free(legal_moves);

    if (!boardpos_eq(best_move.from, NULL_BOARDPOS)) {
        // Add the principal variation (best move) to the transposition table, so that it can be used in move
        // ordering and by generate_move.
        struct TranspositionEntry entry = tptable_get(state->hash);
        entry.hash = state->hash;
        entry.depth = depth;
        entry.best_move = best_move;
        entry.value = best_value;
        entry.type = EntryTypeExact;
        tptable_put(entry);
        printf("[movegen] **** %d %d\n", depth, best_value);
    }
}

struct MovegenTaskArg {
    struct AtomicCounter *refcount;  // Refcount of state and legal_moves.
    struct GameState *state;
    struct Move *legal_moves;
    unsigned int move_count;
    int depth;
    time_t start_time;
};

// Task executed the thread pool to perform move generation.
// See generate_move.
static bool movegen_task(struct MovegenTaskArg *arg) {
    negamax_from_root(arg->state, arg->depth, arg->start_time);

    if (acnt_dec(arg->refcount)) {
        free(arg->state);
        free(arg->legal_moves);
    }

    free(arg);
    return true;
}

// Generate the best move for the player to move, using negamax with iterative deepening and Lazy SMP on supported
// systems.
// The best move will be stored in the transposition table.
// On systems with multithreading support the function will not block.
void generate_move(struct GameState *state, struct ThreadPool *pool, time_t start_time) {
    // Prevent entries for this hash being replaced by other hashes.
    tptable_set_protected_hash(state->hash);

    // Check if there is a move available in the opening book if we are on move <= 5.
    if (state->move_count <= 5) {
        struct OpeningItem *opening = find_opening_by_hash(state->hash);
        if (opening) {
            // If there are multiple moves available then one is chosen at random.
            struct Move move = opening->moves[rand() % opening->moves_count];

            // Ensure the move is legal to reduce the impact of Zobrist hash collisions.
            if (is_move_legal(state, move)) {
                struct TranspositionEntry entry = tptable_get(state->hash);
                entry.hash = state->hash;
                entry.best_move = move;
                entry.depth = CHAR_MAX;
                entry.value = 0;
                entry.type = EntryTypeExact;
                tptable_put(entry);
                return;
            }
        }
    }

    // The threads will need a copy of the gamestate incase it is deallocated.
    struct GameState *state_for_threads = copy_gamestate(state);

    // Find the legal moves now to avoid doing it on each thread.
    struct Move *legal_moves;
    unsigned int move_count =
        all_legal_moves_ordered(state, state->white_to_move ? WhitePlayer : BlackPlayer, &legal_moves);

    // Counts references to the legal moves and the copied gamestate.
    struct AtomicCounter *refcount = acnt_init(MAX_SEARCH_DEPTH);

    // Use iterative deepening to find the best move.
    // Initially the searching algorithm is ran with a low maximum depth. This depth is iteratively increased and the
    // algorithm is reran, until a time limit is reached. The result from the last completed search is used as the best
    // move. This allows for searching at the maximum depth (searching at greater depths will yield a more accurate
    // result) while keeping inside a time limit, so that the move is returned within a certain time on all hardware.
    // The performance effect of rerunning the algorithm multiple times instead of running it once is minimised as a
    // transposition table is used to store the results of previous evaluations, and the principal variation is stored
    // which is used in move ordering to improve alpha-beta pruning performance.
    for (int depth = 1; depth <= MAX_SEARCH_DEPTH; depth++) {
        // Freed by thread
        struct MovegenTaskArg *arg = malloc(sizeof(*arg));
        arg->state = state_for_threads;
        arg->depth = depth;
        arg->start_time = start_time;
        arg->legal_moves = legal_moves;
        arg->move_count = move_count;
        arg->refcount = refcount;

        threadpool_enqueue(pool, (TaskFunc)movegen_task, arg);
    }
}

// Checks if the game is stalemate.
// The game is stalemate when the player to move has no possible legal moves, but is not in check.
bool is_stalemate(struct GameState *state) {
    enum Player to_move = state->white_to_move ? WhitePlayer : BlackPlayer;

    // If the king is in check it cannot be stalemate.
    if (is_player_in_check(state, to_move)) return false;

    // If the king is not in check and there are no legal moves it is stalemate.
    struct BoardPos legal_moves[PIECE_LEGAL_MOVES_MAX];
    struct BoardPos *piece_list = to_move == WhitePlayer ? state->piece_list_white : state->piece_list_black;
    for (int i = 0; i < 16; i++) {
        if (!boardpos_eq(piece_list[i], NULL_BOARDPOS)) {
            if (legal_moves_from_pos(state, piece_list[i], legal_moves) != 0) {
                return false;
            }
        }
    }

    // No legal moves were found, it is stalemate.
    return true;
}

// Checks if a player has been checkmated.
bool is_player_checkmated(struct GameState *state, enum Player player) {
    // A player must be in check to be checkmated.
    if (!is_player_in_check(state, player)) return false;

    // A player is checkmated if they are in check and have no legal moves to get themselves out of check.
    // All the player's pieces are checked to see if they have any legal moves.
    struct BoardPos legal_moves[PIECE_LEGAL_MOVES_MAX];
    struct BoardPos *piece_list = player == WhitePlayer ? state->piece_list_white : state->piece_list_black;
    for (int i = 0; i < 16; i++) {
        if (!boardpos_eq(piece_list[i], NULL_BOARDPOS)) {
            if (legal_moves_from_pos(state, piece_list[i], legal_moves) != 0) {
                return false;
            }
        }
    }

    // No legal moves were found, the player is checkmated.
    return true;
}

// Checks if a state is legal, e.g. if white is to move, black's king must not be in check for the state to be legal.
static bool is_state_legal(struct GameState *state) {
    // NOTE This must not use state->hash (may be unset).
    enum Player last_move = state->white_to_move ? BlackPlayer : WhitePlayer;
    return !is_player_in_check(state, last_move);
}

// Makes a move, updating the board and other state such as castling rights and en passant targets.
// If calculate_hash is true the Zobrist hash of the new state will be written to state->hash.
void make_move(struct GameState *state, struct Move move, bool calculate_hash) {
    struct Piece from_piece = get_piece(state, move.from);
    struct Piece to_piece = get_piece(state, move.to);

    if (from_piece.type == Pawn) {
        if ((abs(move.from.rank - move.to.rank) == 2)) {
            // Add en passant target files on double pawn push.
            if (from_piece.player == WhitePlayer) {
                state->enpassant_target_black = move.from.file;
            } else {
                state->enpassant_target_white = move.from.file;
            }
        } else {
            if (move.from.file != move.to.file && to_piece.type == Empty) {
                // Perform en passant capture.
                put_piece(state, Piece(Empty, WhitePlayer), BoardPos(move.to.file, move.from.rank));
                // Remove en passant captured piece from piece list
                change_piece_list_pos(state, other_player(from_piece.player), BoardPos(move.to.file, move.from.rank),
                                      NULL_BOARDPOS);
            }

            // Remove en passant target if a pawn on the file does not double push.
            unset_enpassant_target_file(state, other_player(from_piece.player));
        }
    } else if (from_piece.type == Rook && (boardpos_eq(move.from, ROOK_STARTING_POSITIONS_LEFT[from_piece.player]) ||
                                           boardpos_eq(move.from, ROOK_STARTING_POSITIONS_RIGHT[from_piece.player]))) {
        // Remove castling rights if a rook moves from its starting position.
        if (move.from.file == 0)
            unset_castlert_left(state, from_piece.player);
        else if (move.from.file == 7)
            unset_castlert_right(state, from_piece.player);
    } else if (to_piece.type == Rook && (boardpos_eq(move.to, ROOK_STARTING_POSITIONS_LEFT[to_piece.player]) ||
                                         boardpos_eq(move.to, ROOK_STARTING_POSITIONS_RIGHT[to_piece.player]))) {
        // Remove castling rights if a rook is captured at its starting position.
        if (move.to.file == 0)
            unset_castlert_left(state, to_piece.player);
        else if (move.to.file == 7)
            unset_castlert_right(state, to_piece.player);
    } else if (from_piece.type == King) {
        // Remove all castling rights if the king moves.
        unset_castlert_left(state, from_piece.player);
        unset_castlert_right(state, from_piece.player);

        // Move the rook if the move is castling.
        if (boardpos_eq(move.from, KING_STARTING_POSITIONS[from_piece.player])) {
            if (move.to.file == 2) {
                move_piece(state, BoardPos(0, move.from.rank), BoardPos(3, move.from.rank));
                change_piece_list_pos(state, from_piece.player, BoardPos(0, move.from.rank),
                                      BoardPos(3, move.from.rank));
            } else if (move.to.file == 6) {
                put_piece(state, Piece(Empty, WhitePlayer), BoardPos(7, move.from.rank));
                put_piece(state, Piece(Rook, from_piece.player), BoardPos(5, move.from.rank));
                change_piece_list_pos(state, from_piece.player, BoardPos(7, move.from.rank),
                                      BoardPos(5, move.from.rank));
            }
        }

        // Update the king position.
        set_king_pos(state, from_piece.player, move.to);
    }

    // Update the piece list.
    change_piece_list_pos(state, from_piece.player, move.from, move.to);
    if (to_piece.type != Empty) {
        change_piece_list_pos(state, to_piece.player, move.to, NULL_BOARDPOS);
    }

    // Remove en passant target file, the player has moved.
    unset_enpassant_target_file(state, from_piece.player);

    // If the move is a promotion the new piece will be a queen.
    struct Piece new_piece = from_piece;
    if (from_piece.type == Pawn && (move.to.rank == 0 || move.to.rank == 7)) {
        new_piece = Piece(Queen, from_piece.player);
    }

    // Move the piece.
    put_piece(state, new_piece, move.to);
    put_piece(state, Piece(Empty, WhitePlayer), move.from);

    // Update check status.
    state->black_king_in_check = is_piece_attacked(state, state->black_king, WhitePlayer);
    state->white_king_in_check = is_piece_attacked(state, state->white_king, BlackPlayer);

    // Update move count.
    state->move_count += 1;

    // A move has been made so the player to move swaps.
    state->white_to_move = !state->white_to_move;

    // Calculate the Zobrist hash of the new state if needed.
    if (calculate_hash) {
        state->hash = hash_state(state);
    } else {
        state->hash = 0;
    }
}

// Returns if it is possible for a piece to move from one position to another.
// It does not check legality (e.g. if in check, castling rights), it only checks if a the move follows the patterns of
// the piece.
// Also checks if any squares under a castling moves are being attacked.
static bool is_move_possible(struct GameState *state, struct Move move) {
    struct Piece from_piece = get_piece(state, move.from);
    struct Piece to_piece = get_piece(state, move.to);

    // A piece cannot move to a square occupied by the same player.
    if (to_piece.type != Empty && from_piece.player == to_piece.player) return false;

    // Check if the move follows the pieces pattern (e.g diagonal only for bishop) and if there are any pieces in the
    // way.
    switch (from_piece.type) {
        case King:
            // The king can normally move a maximum of one square in any direction.
            if (abs(move.from.file - move.to.file) <= 1 && abs(move.from.rank - move.to.rank) <= 1) {
                return true;
            }

            // Check if the move is a castling move.
            bool is_castle = move.from.rank == move.to.rank && (move.to.file == 6 || move.to.file == 2) &&
                             boardpos_eq(move.from, KING_STARTING_POSITIONS[from_piece.player]);
            if (is_castle) {
                int direction = move.to.file == 6 ? 1 : -1;

                // The last file which will be checked in the loop below.
                int last_checked_file = move.to.file == 6 ? 6 : 1;

                // Make sure all squares between the king and the destination are empty and not under attack.
                for (int i = 4; i != last_checked_file + direction; i += direction) {
                    struct Piece check = get_piece(state, BoardPos(i, move.from.rank));

                    // All squares between the castling move must be empty, except the king itself.
                    if (check.type != Empty && i != 4) return false;

                    // All squares between the castling move must not be attacked by the enemy, except for queenside
                    // castling where file 1 can be attacked.
                    if (i != 1 &&
                        is_piece_attacked(state, BoardPos(i, move.from.rank), other_player(from_piece.player)))
                        return false;
                }
                return true;
            }

            return false;
        case Queen:
        case Rook:
        case Bishop:
            if (move.from.file == move.to.file) {
                // Vertical movement - only the queen and rook can move vertically.
                if (from_piece.type == Bishop) return false;

                // Make sure there are no pieces in the way of the move.
                for (int i = MIN(move.from.rank, move.to.rank) + 1; i < MAX(move.from.rank, move.to.rank); i++) {
                    struct Piece check = get_piece(state, BoardPos(move.from.file, i));
                    if (check.type != Empty) return false;
                }

                return true;
            } else if (move.from.rank == move.to.rank) {
                // Horizontal movement - only the queen and rook can move horizontally.
                if (from_piece.type == Bishop) return false;

                // Make sure there are no pieces in the way of the move.
                for (int i = MIN(move.from.file, move.to.file) + 1; i < MAX(move.from.file, move.to.file); i++) {
                    struct Piece check = get_piece(state, BoardPos(i, move.from.rank));
                    if (check.type != Empty) return false;
                }

                return true;
            } else if (abs(move.from.file - move.to.file) == abs(move.from.rank - move.to.rank)) {
                // Diagonal movement - only the queen and bishop can move diagonally.
                if (from_piece.type == Rook) return false;

                // Make sure there are no pieces in the way of the move.
                int file_add = move.from.file > move.to.file ? -1 : 1;
                int rank_add = move.from.rank > move.to.rank ? -1 : 1;
                int file = move.from.file + file_add;
                int rank = move.from.rank + rank_add;
                while (file != move.to.file) {
                    struct Piece check = get_piece(state, BoardPos(file, rank));
                    if (check.type != Empty) return false;

                    file += file_add;
                    rank += rank_add;
                }

                return true;
            }

            return false;
        case Knight:
            // The knight must move in an 'L' shape.
            // A knight can move over the top of other pieces, so no need to check if there are pieces in the way.
            return (abs(move.from.file - move.to.file) == 2 && abs(move.from.rank - move.to.rank) == 1) ||
                   (abs(move.from.file - move.to.file) == 1 && abs(move.from.rank - move.to.rank) == 2);
        case Pawn:;
            // A pawn can move one or two squares vertically in the direction towards the other player,
            // or one square diagonally in the direction towards the other player.
            // When moving two squares vertically it must not 'jump' over a piece.
            int direction = from_piece.player == BlackPlayer ? 1 : -1;

            return (move.to.rank - move.from.rank == direction && abs(move.from.file - move.to.file) <= 1) ||
                   (move.to.rank - move.from.rank == 2 * direction && move.from.file == move.to.file);
        case Empty:
            return false;
    }

    return false;
}

// Checks if a move is legal.
bool is_move_legal(struct GameState *state, struct Move move) {
    // First check if the move follows the move patterns of the piece being moved.
    if (!is_move_possible(state, move)) return false;

    struct Piece from_piece = get_piece(state, move.from);
    struct Piece to_piece = get_piece(state, move.to);

    // The king may not be captured.
    if (to_piece.type == King) return false;

    // The move must be made by the player to move.
    if (from_piece.player != (state->white_to_move ? WhitePlayer : BlackPlayer)) return false;

    if (from_piece.type == Pawn) {
        if (move.from.file != move.to.file) {
            // Check en passant legality.
            // The moved to file must be an en passant target file, and the rank must be a valid en passant rank.
            if (to_piece.type == Empty) {
                if ((from_piece.player == WhitePlayer && move.from.rank != 3) ||
                    (from_piece.player == BlackPlayer && move.from.rank != 4) ||
                    (get_enpassant_target_file(state, from_piece.player) != move.to.file))
                    return false;
            }
        } else if (abs(move.from.rank - move.to.rank) == 2) {
            // Check double pawn push legality.

            // A double pawn push can only be made from the pawn's starting position.
            if (from_piece.player == BlackPlayer && move.from.rank != 1) return false;
            if (from_piece.player == WhitePlayer && move.from.rank != 6) return false;

            // A double pawn push cannot "jump over" a piece.
            int max_rank = MAX(move.to.rank, move.from.rank);
            struct Piece piece1 = get_piece(state, BoardPos(move.from.file, max_rank - 1));
            struct Piece piece2 = get_piece(state, BoardPos(move.from.file, move.to.rank));
            if (piece1.type != Empty || piece2.type != Empty) return false;
        } else {
            // A "normal" pawn push must not land on an occupied square.
            struct Piece piece = get_piece(state, move.to);
            if (piece.type != Empty) return false;
        }
    } else if (from_piece.type == King && abs(move.from.file - move.to.file) == 2) {
        // Check castling rights.
        if (from_piece.player == WhitePlayer) {
            if ((move.to.file == 2 && !state->white_castlert_left) ||
                (move.to.file == 6 && !state->white_castlert_right))
                return false;
        } else {
            if ((move.to.file == 2 && !state->black_castlert_left) ||
                (move.to.file == 6 && !state->black_castlert_right))
                return false;
        }
    }

    // Check if the resulting state after the move is legal, for example the player which has moved must not be in
    // check.
    struct GameState *state_copy = copy_gamestate(state);
    make_move(state_copy, move, false);
    bool legal = is_state_legal(state_copy);
    free(state_copy);

    return legal;
}

// Checks if a certain player's piece is being attacked, or if an empty square is controlled.
bool is_piece_attacked(struct GameState *state, struct BoardPos attackee_pos, enum Player attacker) {
    struct Piece attackee_piece = get_piece(state, attackee_pos);

    // Each possible attacker piece type is checked, each piece moves differently.
    // For each piece type every valid (regardless of legality) move pattern ending at the attackee position is checked
    // to see if there is an attacking piece of that type in an attacking position.

    // Check King, Rook, Bishop, Queen
    for (int i = 0; i < 8; i++) {
        const struct BoardPos translation = PIECE_MOVE_DIRECTIONS[Queen - 1][i];

        const bool is_diagonal = abs(translation.file) == abs(translation.rank);
        const bool is_king = abs(translation.file) <= 1 && abs(translation.rank) <= 1;

        struct BoardPos check = boardpos_add(attackee_pos, translation);
        while (!boardpos_eq(check, NULL_BOARDPOS)) {
            struct Piece check_piece = get_piece(state, check);

            if (check_piece.type != Empty) {
                // Check if there is an attacking piece of the correct type in the possible attacking
                // position.

                bool correct_piece = check_piece.type == Queen || (is_king && check_piece.type == King) ||
                                     (is_diagonal && check_piece.type == Bishop) ||
                                     (!is_diagonal && check_piece.type == Rook);

                if (correct_piece && check_piece.player == attacker) {
                    return true;
                } else {
                    break;
                }
            }

            // Queens, rooks and bishops can attack over multiple squares in the same direction, so the
            // check continues for every square in that direction. Overflow is handled by boardpos_add,
            // NULL_BOARDPOS will be returned if the add overflows.
            check = boardpos_add(check, translation);
        }
    }

    // Check Pawn
    for (int i = 0; i < 8; i++) {
        // The pawn has less than eight possible move directions, so a NULL_BOARDPOS singifies
        // we have reached the end of all the possible directions.
        if (boardpos_eq(PIECE_MOVE_DIRECTIONS[Pawn - 1][i], NULL_BOARDPOS)) {
            break;
        }

        struct BoardPos direction = PIECE_MOVE_DIRECTIONS[Pawn - 1][i];

        // The pawn only attacks on the two diagonal squares in front of it, so ignore moves which remain in
        // the same file.
        if (direction.file == 0) continue;

        // If it is black attacking the pawns move in the opposite direction.
        if (attackee_piece.player == BlackPlayer) {
            direction.file *= -1;
            direction.rank *= -1;
        }

        struct BoardPos check = boardpos_add(attackee_pos, direction);
        if (!boardpos_eq(check, NULL_BOARDPOS)) {
            // Check if there is an attacking piece of the correct type in the possible attacking
            // position.
            struct Piece check_piece = get_piece(state, check);
            if (check_piece.type == Pawn && check_piece.player == attacker) {
                return true;
            }
        }
    }

    // Check Knight
    for (int i = 0; i < 8; i++) {
        // The knight has less than eight possible move directions, so a NULL_BOARDPOS singifies
        // we have reached the end of all the possible directions.
        if (boardpos_eq(PIECE_MOVE_DIRECTIONS[Knight - 1][i], NULL_BOARDPOS)) {
            break;
        }

        struct BoardPos direction = PIECE_MOVE_DIRECTIONS[Knight - 1][i];
        struct BoardPos check = boardpos_add(attackee_pos, direction);

        if (!boardpos_eq(check, NULL_BOARDPOS)) {
            // Check if there is an attacking piece of the correct type in the possible attacking
            // position.
            struct Piece check_piece = get_piece(state, check);
            if (check_piece.type == Knight && check_piece.player == attacker) {
                return true;
            }
        }
    }

    return false;
}
