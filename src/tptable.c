#include <assert.h>
#include <config.h>
#include <stdio.h>
#include <string.h>
#include <threadpool.h>
#include <tptable.h>
#include <stdlib.h>

// The hash table used to store the entries
static struct TranspositionEntry tp_table[TRANSPOSITION_TABLE_SIZE] = {0};
static ZobristHash protected_hash = 0;

#ifdef HAS_C11_CONCURRENCY
static mtx_t tp_table_mutex;
#endif

#ifdef HAS_C11_CONCURRENCY
static void check_err(int ret) {
    if (ret != thrd_success) {
        puts("tptable c11threads error");
        exit(EXIT_FAILURE);
    }
}
#endif

void tptable_init() {
#ifdef HAS_C11_CONCURRENCY
    check_err(mtx_init(&tp_table_mutex, mtx_plain));
#endif
}

void tptable_deinit() {
#ifdef HAS_C11_CONCURRENCY
    mtx_destroy(&tp_table_mutex);
#endif
}

// Get a move from the transpoition table by a zobrist hash of the state
// Returns a Move with NULL_BOARDPOS as the `from` and 0 as the depth if there is no move for the
// specified hash
struct TranspositionEntry tptable_get(ZobristHash hash) {
#ifdef HAS_C11_CONCURRENCY
    check_err(mtx_lock(&tp_table_mutex));
#endif

    struct TranspositionEntry t = tp_table[hash % TRANSPOSITION_TABLE_SIZE];
    if (t.hash != hash) {
        t = (struct TranspositionEntry){.best_move = (struct Move){NULL_BOARDPOS, NULL_BOARDPOS},
                                        .depth = 0,
                                        .hash = 0,
                                        .type = EntryTypeExact,
                                        .value = 0};
    }

#ifdef HAS_C11_CONCURRENCY
    check_err(mtx_unlock(&tp_table_mutex));
#endif

    return t;
}

// Put a move into the transposition table,
// Replacing an existing move if this move is from a greater or equal `depth`, or if it adds a best move.
void tptable_put(struct TranspositionEntry entry) {
#ifdef HAS_C11_CONCURRENCY
    check_err(mtx_lock(&tp_table_mutex));
#endif

    struct TranspositionEntry prev = tp_table[entry.hash % TRANSPOSITION_TABLE_SIZE];
    if ((prev.hash != protected_hash && entry.hash != prev.hash) ||
        (prev.hash == entry.hash && prev.depth <= entry.depth)) {
        tp_table[entry.hash % TRANSPOSITION_TABLE_SIZE] = entry;
    }

#ifdef HAS_C11_CONCURRENCY
    check_err(mtx_unlock(&tp_table_mutex));
#endif
}

// Clears all entries from the transposition table
void tptable_clear() {
#ifdef HAS_C11_CONCURRENCY
    check_err(mtx_lock(&tp_table_mutex));
#endif

    memset((void*)tp_table, 0, TRANSPOSITION_TABLE_SIZE * sizeof(struct TranspositionEntry));

#ifdef HAS_C11_CONCURRENCY
    check_err(mtx_unlock(&tp_table_mutex));
#endif
}

// The entry for a protected hash can only be replaced by an entry with the same hash.
// Usually if (hash % TRANSPOSITION_TABLE_SIZE) is the same for two hashes one will replace the other.
// If the entry for hash has another hash in it the entry will be reset with the inputted hash.
void tptable_set_protected_hash(ZobristHash hash) {
#ifdef HAS_C11_CONCURRENCY
    check_err(mtx_lock(&tp_table_mutex));
#endif

    protected_hash = hash;
    struct TranspositionEntry entry = tp_table[hash % TRANSPOSITION_TABLE_SIZE];
    if (entry.hash != hash) {
        entry.hash = hash;
        entry.depth = 0;
        entry.value = 0;
        entry.best_move = (struct Move){NULL_BOARDPOS, NULL_BOARDPOS};
        tp_table[hash % TRANSPOSITION_TABLE_SIZE] = entry;
    }

#ifdef HAS_C11_CONCURRENCY
    check_err(mtx_unlock(&tp_table_mutex));
#endif
}