/* moves.c -- enumerate moves */

#include <assert.h>

#include "puzzle.h"
#include "tileset.h"

/*
 * List of all possible moves for a given position of the empty square.
 * There are up to four moves from every square, if there are less, the
 * remainder is filled up with -1.
 */
const signed char movetab[TILE_COUNT][4] = {
	 1,  5, -1, -1,
	 0,  2,  6, -1,
	 1,  3,  7, -1,
	 2,  4,  8, -1,
	 3,  9, -1, -1,

	 0,  6, 10, -1,
	 1,  5,  7, 11,
	 2,  6,  8, 12,
	 3,  7,  9, 13,
	 4,  8, 14, -1,

	 5, 11, 15, -1,
	 6, 10, 12, 16,
	 7, 11, 13, 17,
	 8, 12, 14, 18,
	 9, 13, 19, -1,

	10, 16, 20, -1,
	11, 15, 17, 21,
	12, 16, 18, 22,
	13, 17, 19, 23,
	14, 18, 24, -1,

	15, 21, -1, -1,
	16, 20, 22, -1,
	17, 21, 23, -1,
	18, 22, 24, -1,
	19, 23, -1, -1,
};

/*
 * Generate all moves that lead from any partial puzzle configuration
 * with equivalence class eq to a different equivalence class.  Return
 * the number of moves generated and store the moves in moves.
 */
extern size_t
generate_moves(struct move moves[MAX_MOVES], tileset eq)
{
	size_t n_moves = 0, i, zloc;
	tileset req;
	const signed char *dests;

	for (req = tileset_reduce_eqclass(eq); !tileset_empty(req); req = tileset_remove_least(req)) {
		zloc = tileset_get_least(req);
		dests = get_moves(zloc);

		for (i = 0; i < 4; i++) {
			if (dests[i] == -1)
				break;

			if (!tileset_has(eq, dests[i])) {
				moves[n_moves].zloc = zloc;
				moves[n_moves++].dest = dests[i];
			}
		}
	}

	assert(n_moves <= MAX_MOVES);
	return (n_moves);
}
