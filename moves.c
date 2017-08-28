/* moves.c -- enumerate moves */

#include <assert.h>

#include "tileset.h"

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
