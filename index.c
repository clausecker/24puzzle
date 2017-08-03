/* index.c -- compute puzzle indices */
#include <stddef.h>

#include "builtins.h"
#include "tileset.h"
#include "index.h"

/*
 * Compute the index for tiles ts in p and store them in idx.
 * It is assumed that p is a valid puzzle.
 */
extern void
compute_index(tileset ts, struct index *idx, const struct puzzle *p)
{
	size_t i;
	tileset occupation = FULL_TILESET;

	for (i = 0; !tileset_empty(ts); ts = tileset_remove_least(ts), i++) {
		idx->cmp[i] = tileset_count(tileset_intersect(occupation,
		    tileset_least(p->tiles[tileset_get_least(ts)])));
		occupation = tileset_remove(occupation, p->tiles[i]);
	}
}
