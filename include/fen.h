#ifndef FEN_H
#define FEN_H

#include <chess.h>
#include <stdlib.h>

struct GameState *fen_to_gamestate(const char *fen);
void gamestate_to_fen(struct GameState *state, char *fen);

#endif /* FEN_H */
