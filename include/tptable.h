#ifndef TPTABLE_H
#define TPTABLE_H

#include <chess.h>
#include <zobrist.h>

// For entries with an evaluation value, states whether the value is an upper bound, lower bound, or an exact value.
enum EntryType { EntryTypeExact, EntryTypeUpper, EntryTypeLower };

// An entry in the transposition table
struct TranspositionEntry {
    ZobristHash hash;
    struct Move best_move;  // May be absent (NULL_BOARDPOS as the `from` position)
    unsigned char depth;
    int value;  // May be absent (set to 0, when depth = 0)
    enum EntryType type;
};

struct TranspositionEntry tptable_get(ZobristHash hash);
void tptable_put(struct TranspositionEntry entry);
void tptable_clear();

#endif /* TPTABLE_H */
