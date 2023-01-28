#include <assert.h>
#include <ctype.h>
#include <engine.h>
#include <fen.h>
#include <stdlib.h>
#include <zobrist.h>

// Convert a piece to its FEN representation
// Returns '\0' if the piece is Empty
static char piece_to_fen(struct Piece piece) {
    static const char PIECES[7] = {'\0', 'K', 'Q', 'R', 'B', 'N', 'P'};
    char out = PIECES[piece.type];

    // Black pieces are lowercase
    if (out != '\0' && piece.player == BlackPlayer) {
        out = tolower(out);
    }

    return out;
}

// Returns the piece if it was successfully parsed,
// an Empty piece otherwise
static struct Piece parse_fen_piece(char piece_char) {
    struct Piece out_piece = Piece(Empty, WhitePlayer);

    // Lowercase chars are black pieces,
    // convert them to uppercase so they can be
    // matched below
    if (islower(piece_char)) {
        out_piece.player = BlackPlayer;
        piece_char = toupper(piece_char);
    }

    switch (piece_char) {
        case 'P':
            out_piece.type = Pawn;
            break;
        case 'N':
            out_piece.type = Knight;
            break;
        case 'B':
            out_piece.type = Bishop;
            break;
        case 'R':
            out_piece.type = Rook;
            break;
        case 'Q':
            out_piece.type = Queen;
            break;
        case 'K':
            out_piece.type = King;
            break;
    }

    return out_piece;
}

// Converts an algebraic notation file character to an integer
// Returns -1 if the char is invalid
static inline int algebraic_file_to_int(char file) {
    if (file < 'a' || file > 'h') {
        return -1;
    } else {
        return file - 'a';
    }
}

// Convert an integer file to its algebraic notation equivalent
// Assumes the file is valid
static inline char int_file_to_algebraic(int file) {
    assert(file >= 0 && file < 8);
    return file + 'a';
}

// Parse a FEN string into a GameState.
// The returned GameState must be deallocated with deinit_gamestate.
// Returns NULL if there was an error, e.g. invalid FEN.
// https://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation
struct GameState *fen_to_gamestate(const char *fen) {
    struct GameState *out = init_gamestate();
    clear_board(out);

    // Parse piece placement
    int file = 0;
    int rank = 0;
    int i = 0;
    int piece_list_idx_white = 0;
    int piece_list_idx_black = 0;
    for (;; i++) {
        // Unexpected end of string
        if (fen[i] == '\0') {
            goto exit_error;
        }

        // Try parse a FEN piece character, e.g. K = white king
        struct Piece piece = parse_fen_piece(fen[i]);

        if (piece.type != Empty) {
            // A piece was successfully parsed, add it to the board

            // The maximum file is 7
            if (file == 8) {
                goto exit_error;
            }

            out->board[file++][rank] = piece;

            if (piece.type == King) {
                // Cannot have multiple kings
                if (!boardpos_eq(get_king_pos(out, piece.player), NULL_BOARDPOS)) {
                    goto exit_error;
                }

                // Set the king position
                set_king_pos(out, piece.player, BoardPos(file - 1, rank));
            }

            // Add the piece to the piece list, checking the limits
            if (piece.player == WhitePlayer) {
                // White's piece limit has been reached
                if (piece_list_idx_white == sizeof(out->piece_list_white) / sizeof(out->piece_list_white[0])) {
                    goto exit_error;
                }

                out->piece_list_white[piece_list_idx_white++] = BoardPos(file - 1, rank);
            } else {
                // Black's piece limit has been reached
                if (piece_list_idx_black == sizeof(out->piece_list_black) / sizeof(out->piece_list_black[0])) {
                    goto exit_error;
                }

                out->piece_list_black[piece_list_idx_black++] = BoardPos(file - 1, rank);
            }
        } else if (fen[i] == '/') {
            // The '/' character separates the ranks
            rank += 1;
            file = 0;

            // The rank should never reach 8 as it is incremented
            // only when there is a '/' separator, which is only
            // between the ranks, not at the end of each rank.
            if (rank == 8) {
                goto exit_error;
            }
        } else if (fen[i] == ' ') {
            // The space character separates the piece placement
            // from the rest of the string
            break;
        } else if (isdigit(fen[i])) {
            // Digits denote a number of empty spaces on the file

            // Convert the ASCII digit character to an integer,
            // add add it to the file
            file += fen[i] - '0';

            // The file may be 8 at the end of the piece placement section
            // as it is incremented after each square.
            // It cannot exceed 8, as there are 8 squares per file.
            if (file > 8) {
                goto exit_error;
            }
        } else {
            // There was an invalid character
            goto exit_error;
        }
    }

    // If the file and rank arent at the ending values then part of the piece placement
    // was missing, and the FEN is invalid
    if (file != 8 || rank != 7) {
        goto exit_error;
    }

    // A space separates sections of the FEN
    if (fen[i++] != ' ') {
        goto exit_error;
    }

    // Parse player to move
    if (fen[i] == 'w') {
        out->white_to_move = true;
    } else if (fen[i] == 'b') {
        out->white_to_move = false;
    } else {
        goto exit_error;
    }

    i += 1;

    // A space separates sections of the FEN
    if (fen[i++] != ' ') {
        goto exit_error;
    }

    // Parse castling rights
    out->white_castlert_left = false;
    out->white_castlert_right = false;
    out->black_castlert_left = false;
    out->black_castlert_right = false;

    if (fen[i] == '-') {
        // Neither side have castling rights
        // This is already set above as the starting case
        i += 1;
    } else {
        // Parse the rights, capital letters are white's rights,
        // lowercase are black's. K = kingside castling, Q = queenside castling

        if (fen[i] == 'K') {
            out->white_castlert_right = true;
            i += 1;
        }

        if (fen[i] == 'Q') {
            out->white_castlert_left = true;
            i += 1;
        }

        if (fen[i] == 'k') {
            out->black_castlert_right = true;
            i += 1;
        }

        if (fen[i] == 'q') {
            out->black_castlert_left = true;
            i += 1;
        }
    }

    // A space separates sections of the FEN
    if (fen[i++] != ' ') {
        goto exit_error;
    }

    // Parse en passant target square
    if (fen[i] == '-') {
        i += 1;
    } else {
        int file = algebraic_file_to_int(fen[i++]);
        if (file == -1) {
            goto exit_error;
        }

        if (fen[i] == '3') {
            out->enpassant_target_black = file;
        } else if (fen[i] == '6') {
            out->enpassant_target_white = file;
        } else {
            goto exit_error;
        }

        i += 1;
    }

    // A space separates sections of the FEN
    if (fen[i++] != ' ') {
        goto exit_error;
    }

    // The halfmove clock and fullmove number would usually follow,
    // but they are not used in this game so are ignored.

    // Check that there are two kings on the board
    if (boardpos_eq(out->white_king, NULL_BOARDPOS) || boardpos_eq(out->black_king, NULL_BOARDPOS)) {
        goto exit_error;
    }

    // Check if the kings are in check
    out->white_king_in_check = is_piece_attacked(out, out->white_king, BlackPlayer);
    out->black_king_in_check = is_piece_attacked(out, out->black_king, WhitePlayer);

    // Add the Zobrist hash
    out->hash = hash_state(out);

    return out;

exit_error:
    // If there was an error the gamestate must be deallocated
    deinit_gamestate(out);
    return NULL;
}

// Serialise a GameState to a FEN string
// `fen` must be at least 90 bytes
void gamestate_to_fen(struct GameState *state, char *fen) {
    int i = 0;

    // Add pieces
    for (int rank = 0; rank < 8; rank++) {
        int empty_count = 0;
        for (int file = 0; file < 8; file++) {
            struct Piece piece = get_piece(state, BoardPos(file, rank));
            if (piece.type != Empty) {
                // If the number of empty squares counted on the file
                // is non-zero, and we are reaching the first non-empty square
                // so add the number of empty squares to the FEN first
                if (empty_count != 0) {
                    fen[i++] = empty_count + '0';
                    empty_count = 0;
                }

                // Add the piece to the FEN
                fen[i++] = piece_to_fen(piece);
            } else {
                empty_count++;
            }

            // A series of empty squares on a file are represented by
            // a digit representing the number of empty squares
            if (file == 7 && empty_count != 0) {
                fen[i++] = empty_count + '0';
                empty_count = 0;
            }
        }

        // If we have added the final rank then terminate the section
        // with a space, otherwise add a '/' to separate the ranks
        if (rank == 7) {
            fen[i++] = ' ';
        } else {
            fen[i++] = '/';
        }
    }

    // Add player to move
    fen[i++] = state->white_to_move ? 'w' : 'b';
    fen[i++] = ' ';

    // Add castling rights
    if (!state->white_castlert_left && !state->white_castlert_right && !state->black_castlert_left &&
        !state->black_castlert_right) {
        // Neither player having castling rights is represented with a '-'
        fen[i++] = '-';
    } else {
        if (state->white_castlert_right) {
            fen[i++] = 'K';
        }

        if (state->white_castlert_left) {
            fen[i++] = 'Q';
        }

        if (state->black_castlert_right) {
            fen[i++] = 'k';
        }

        if (state->black_castlert_left) {
            fen[i++] = 'q';
        }
    }

    // A space separates the sections
    fen[i++] = ' ';

    // Add en passant target square
    int target_file = get_enpassant_target_file(state, state->white_to_move ? WhitePlayer : BlackPlayer);
    if (target_file != -1) {
        // If there is an en passant target file the position behind the pawn
        // which just double pushed is added
        fen[i++] = int_file_to_algebraic(target_file);
        fen[i++] = state->white_to_move ? '6' : '3';
    } else {
        fen[i++] = '-';
    }

    // A space separates the sections
    fen[i++] = ' ';

    // The halfmove clock and fullmove number are not used
    // in this game, so are both set to zero
    fen[i++] = '0';
    fen[i++] = ' ';
    fen[i++] = '0';

    // Add NUL terminator
    fen[i++] = '\0';
}