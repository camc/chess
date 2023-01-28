#include <chess.h>
#include <engine.h>
#include <stdbool.h>

static int test_is_piece_attacked_King() {
    struct BoardPos white_piece = BoardPos(4, 7);
    struct BoardPos black_king = BoardPos(4, 6);

    struct GameState *state = init_gamestate();
    bool r1 = is_piece_attacked(state, white_piece, BlackPlayer);
    put_piece(state, Piece(King, BlackPlayer), black_king);
    bool r2 = is_piece_attacked(state, white_piece, BlackPlayer);

    deinit_gamestate(state);

    TEST_ASSERT(!r1, "expected is_piece_attacked() == false, got true");
    TEST_ASSERT(r2, "expected is_piece_attacked() == true, got false");
    return 0;
}

static int test_is_piece_attacked_Queen() {
    struct BoardPos white_piece = BoardPos(2, 4);
    struct BoardPos black_queen = BoardPos(4, 6);

    struct GameState *state = init_gamestate();
    bool r1 = is_piece_attacked(state, white_piece, BlackPlayer);
    put_piece(state, Piece(Queen, BlackPlayer), black_queen);
    bool r2 = is_piece_attacked(state, white_piece, BlackPlayer);

    deinit_gamestate(state);

    TEST_ASSERT(!r1, "expected is_piece_attacked() == false, got true");
    TEST_ASSERT(r2, "expected is_piece_attacked() == true, got false");
    return 0;
}

static int test_is_piece_attacked_Rook() {
    struct BoardPos white_piece = BoardPos(2, 4);
    struct BoardPos black_rook = BoardPos(2, 6);

    struct GameState *state = init_gamestate();
    bool r1 = is_piece_attacked(state, white_piece, BlackPlayer);
    put_piece(state, Piece(Rook, BlackPlayer), black_rook);
    bool r2 = is_piece_attacked(state, white_piece, BlackPlayer);

    deinit_gamestate(state);

    TEST_ASSERT(!r1, "expected is_piece_attacked() == false, got true");
    TEST_ASSERT(r2, "expected is_piece_attacked() == true, got false");
    return 0;
}

static int test_is_piece_attacked_Bishop() {
    struct BoardPos white_piece = BoardPos(2, 4);
    struct BoardPos black_bishop = BoardPos(4, 6);

    struct GameState *state = init_gamestate();
    bool r1 = is_piece_attacked(state, white_piece, BlackPlayer);
    put_piece(state, Piece(Bishop, BlackPlayer), black_bishop);
    bool r2 = is_piece_attacked(state, white_piece, BlackPlayer);

    deinit_gamestate(state);

    TEST_ASSERT(!r1, "expected is_piece_attacked() == false, got true");
    TEST_ASSERT(r2, "expected is_piece_attacked() == true, got false");
    return 0;
}

static int test_is_piece_attacked_Knight() {
    struct BoardPos white_piece = BoardPos(2, 4);
    struct BoardPos black_knight = BoardPos(3, 6);

    struct GameState *state = init_gamestate();
    bool r1 = is_piece_attacked(state, white_piece, BlackPlayer);
    put_piece(state, Piece(Knight, BlackPlayer), black_knight);
    bool r2 = is_piece_attacked(state, white_piece, BlackPlayer);

    deinit_gamestate(state);

    TEST_ASSERT(!r1, "expected is_piece_attacked() == false, got true");
    TEST_ASSERT(r2, "expected is_piece_attacked() == true, got false");
    return 0;
}

DEF_TEST_MODULE(engine, test_is_piece_attacked_King, test_is_piece_attacked_Queen, test_is_piece_attacked_Rook,
                test_is_piece_attacked_Bishop, test_is_piece_attacked_Knight);
