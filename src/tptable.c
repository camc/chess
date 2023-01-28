#include <config.h>
#include <string.h>
#include <tptable.h>

// The hash table used to store the entries
static struct TranspositionEntry tp_table[TRANSPOSITION_TABLE_SIZE] = {0};

// Get a move from the transpoition table by a zobrist hash of the state
// Returns a Move with NULL_BOARDPOS as the `from` and 0 as the depth if there is no move for the
// specified hash
struct TranspositionEntry tptable_get(ZobristHash hash) {
    struct TranspositionEntry t = tp_table[hash % TRANSPOSITION_TABLE_SIZE];
    if (t.hash == hash) {
        return t;
    } else {
        return (struct TranspositionEntry){.best_move = (struct Move){NULL_BOARDPOS, NULL_BOARDPOS},
                                           .depth = 0,
                                           .hash = 0,
                                           .type = EntryTypeExact,
                                           .value = 0};
    }
}

// Gets a move for `hash` if it exists, and returns the depth it is from
// Returns -1 if no move exists for the hash
static int tptable_getdepth(ZobristHash hash) {
    struct TranspositionEntry t = tp_table[hash % TRANSPOSITION_TABLE_SIZE];
    if (t.hash == hash && t.depth != 0) {
        return t.depth;
    } else {
        return -1;
    }
}

// Put a move into the transposition table,
// Replacing an existing move if this move is from a greater or equal `depth`
void tptable_put(struct TranspositionEntry entry) {
    if (tptable_getdepth(entry.hash) <= entry.depth) {
        tp_table[entry.hash % TRANSPOSITION_TABLE_SIZE] = entry;
    }
}

// Clears all entries from the transposition table
void tptable_clear() { memset(tp_table, 0, TRANSPOSITION_TABLE_SIZE * sizeof(struct TranspositionEntry)); }