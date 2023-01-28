#include <fen.h>
#include <string.h>
#include <zobrist.h>

static int test_parse_normal() {
    static const char *test_string = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 0";

    struct GameState *state = fen_to_gamestate(test_string);
    bool is_null = state == NULL;
    if (!is_null) {
        TEST_ASSERT(hash_state(state) == 0x463b96181691fc9cULL,
                    "zobrist hash of the state parsed from FEN does not match expected value");
        deinit_gamestate(state);
    }

    TEST_ASSERT(!is_null, "unexpected error in fen_to_gamestate");

    return 0;
}

static int test_serialize_normal() {
    struct GameState *starting_state = init_gamestate();
    char fen[90];
    gamestate_to_fen(starting_state, fen);
    deinit_gamestate(starting_state);
    TEST_ASSERT(strncmp(fen, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 0", 57) == 0,
                "the serialised FEN of the starting GameState does not match the expected value");
    return 0;
}

DEF_TEST_MODULE(fen, test_parse_normal, test_serialize_normal);
