#include <assert.h>
#include <openings.h>
#include <stdio.h>
#include <stdlib.h>

// Stores the moves in the opening list.
// Implemented as a dynamically allocated array as the number of opening moves in the book is unknown,
// so using a hash table would be difficult and there would be a chance of collisions.
// The items list is binary searched, since the list is sorted by hash. Binary search has
// an O(log n) worst case time complexity which is more than efficient enough for the expected number
// of items (< 1 million).
static struct OpeningItem *items = NULL;
static const unsigned int ITEMS_MAX = INT32_MAX;
static unsigned int items_count = 0;

// Binary search the opening items, returns NULL if the item is not found.
// High must be < items_count.
static struct OpeningItem *binary_search_items(uint64_t hash, unsigned int low, unsigned int high) {
    // Ensure overflow is not possible in debug builds.
    assert(high < items_count);
    assert(2 * (unsigned int)ITEMS_MAX - 2 <= UINT32_MAX);

    while (low <= high) {
        // SAFETY: high and low must be less than (2^31 - 1) as ITEMS_MAX is INT32_MAX (2^31 - 1).
        // So (high + low) will be at most (2^32 - 4) which is within the range of an unsigned integer.
        unsigned int mid = (high + low) / 2;
        if (items[mid].hash == hash) {
            return &items[mid];
        } else if (items[mid].hash < hash) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }

    return NULL;
}

// Finds a OpeningItem by hash, returns NULL if the hash is not found
// The OpeningItem returned will be deallocated if deinit_opening_book is called
struct OpeningItem *find_opening_by_hash(uint64_t hash) {
    if (items_count == 0)
        return NULL;
    else
        return binary_search_items(hash, 0, items_count - 1);
}

// Converts a big endian u64 to the host byte order
// These functions are reimplemented as they are not standard functions
// available on all platforms.
static uint64_t ntohll(uint64_t n) {
    unsigned char *np = (unsigned char *)&n;
    return ((uint64_t)np[0] << 56) | ((uint64_t)np[1] << 48) | ((uint64_t)np[2] << 40) | ((uint64_t)np[3] << 32) |
           ((uint64_t)np[4] << 24) | ((uint64_t)np[5] << 16) | ((uint64_t)np[6] << 8) | (uint64_t)np[7];
}

// Converts a big endian u16 to the host byte order
static uint16_t ntohs(uint16_t n) {
    unsigned char *np = (unsigned char *)&n;
    return ((uint16_t)np[0] << 8) | (uint16_t)np[1];
}

// Initialise the opening book from the opening book file.
// Allocated memory must be deallocated using deinit_opening_book().
// The book is in Polyglot BIN format: http://hgm.nubati.net/book_format.html
void init_opening_book() {
    // The size of the structure used to represent an opening in the book, in bytes.
    const size_t ENTRY_SIZE = 16;

    // Each value in the move bit field is 3 bits wide.
    const uint16_t MOVE_VALUE_MASK = 0x7;

    // The offsets by which the bit field must be right logically shifted to get the value as the least significant 3
    // bits.
    const uint16_t TO_FILE_SHIFT = 0;
    const uint16_t TO_ROW_SHIFT = 3;
    const uint16_t FROM_FILE_SHIFT = 6;
    const uint16_t FROM_ROW_SHIFT = 9;
    const uint16_t PROMO_PIECE_SHIFT = 12;

    // The values used to represent pawn promotion in the promo_piece value.
    // Only queen promotion is handled.
    const int PROMO_PIECE_NONE = 0;
    const int PROMO_PIECE_QUEEN = 4;

    size_t items_size = 100;
    items = malloc(sizeof(struct OpeningItem) * items_size);

    FILE *book_file = fopen("res/opening_book.bin", "rb");
    if (!book_file) {
        perror("error initialising opening book");
        exit(EXIT_FAILURE);
    }

    char entry[16];
    while (fread(entry, ENTRY_SIZE, 1, book_file) == 1) {
        // Stop reading new items if the maximum number was reached.
        if (items_count == ITEMS_MAX) {
            break;
        }

        // Extend the dynamic array if it is full.
        if (items_count == items_size) {
            items = realloc(items, sizeof(struct OpeningItem) * items_size * 2);
            items_size *= 2;
        }

        // Load the values from the move entry.
        // The first 8 bytes is the Zobrist hash of the position, the next 2 bytes is the response move.
        uint64_t hash = ntohll(*(uint64_t *)entry);
        uint16_t move = ntohs(*(uint16_t *)(entry + 8));
        int to_file = (move >> TO_FILE_SHIFT) & MOVE_VALUE_MASK;
        int to_row = (move >> TO_ROW_SHIFT) & MOVE_VALUE_MASK;
        int from_file = (move >> FROM_FILE_SHIFT) & MOVE_VALUE_MASK;
        int from_row = (move >> FROM_ROW_SHIFT) & MOVE_VALUE_MASK;
        int promo_piece = (move >> PROMO_PIECE_SHIFT) & MOVE_VALUE_MASK;

        // Promoting to pieces other than queen is unsupported.
        if (promo_piece != PROMO_PIECE_NONE && promo_piece != PROMO_PIECE_QUEEN) {
            continue;
        }

        // Row 0 is the bottom in the Polyglot BIN format (backwards compared to the scheme used for BoardPos).
        struct BoardPos from = BoardPos(from_file, 7 - from_row);
        struct BoardPos to = BoardPos(to_file, 7 - to_row);
        struct Move actual_move = {from, to};

        if (items_count != 0 && items[items_count - 1].hash == hash) {
            struct OpeningItem *item = &items[items_count - 1];

            // The move count is stored as a uint8, so a max
            // of 255 moves per hash can be stored
            if (item->moves_count == UINT8_MAX) {
                continue;
            }

            item->moves_count += 1;
            item->moves = realloc(item->moves, sizeof(struct Move) * item->moves_count);
            item->moves[item->moves_count - 1] = actual_move;
        } else {
            struct OpeningItem item = {.hash = hash, .moves = NULL, .moves_count = 1};
            item.moves = malloc(sizeof(struct Move));
            item.moves[0] = actual_move;
            items[items_count++] = item;
        }
    }

    fclose(book_file);

    // If no items were found in the book it is invalid.
    if (items_count == 0) {
        fprintf(stderr, "error: invalid opening book file");
        exit(EXIT_FAILURE);
    }

    // Shrink the items array down to the minimum size.
    items = realloc(items, sizeof(struct OpeningItem) * items_count);
}

// Deinitialise and deallocate the opening book currently loaded
void deinit_opening_book() {
    for (unsigned int i = 0; i < items_count; i++) {
        free(items[i].moves);
    }

    free(items);
    items = NULL;
    items_count = 0;
}
