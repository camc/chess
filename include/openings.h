#ifndef OPENINGS_H
#define OPENINGS_H

#include <chess.h>
#include <zobrist.h>

struct OpeningItem {
    ZobristHash hash;
    struct Move* moves;
    uint8_t moves_count;
};

void init_opening_book();
void deinit_opening_book();
struct OpeningItem* find_opening_by_hash(ZobristHash hash);

#endif /* OPENINGS_H */
