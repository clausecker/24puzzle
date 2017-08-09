/* tileset.c -- dealing with sets of tiles */

#include "puzzle.h"
#include "tileset.h"

/*
 * Given a tileset cmap representing free positions on the grid and
 * a square number t set in cmap, return a tileset representing
 * all squares reachable from t through members of ts.
 */
static tileset
tileset_flood(tileset cmap, unsigned t)
{
	tileset r = tileset_add(EMPTY_TILESET, t), oldr;

	do {
		oldr = r;
		r = cmap & (r | r  << 5 | r << 1 | r >> 5 | r >> 1);
	} while (oldr != r);

	return (r);
}

/*
 * Given a tileset and a puzzle configuration, compute a tileset
 * representing the squares occupied by tiles in the tileset.
 *
 * TODO: Use SSE4.2 instruction pcmpestrm to compute this quickly.
 */
static tileset
tileset_map(tileset ts, const struct puzzle *p)
{
	tileset map = EMPTY_TILESET;

	for (; !tileset_empty(ts); ts = tileset_remove_least(ts))
		map |= 1 << p->tiles[tileset_get_least(ts)];

	return (map);
}

/*
 * For a tileset ts which may or may not contain the zero tile
 * and a puzzle configuration p, compute a tileset representing
 * the squares we can move the zero tile to without disturbing
 * the non-zero tiles in ts.  If the zero tile is not in ts, it
 * this is just the set of squares not occupied by tiles in ts
 * as we cannot make assumptions about the position of the zero
 * tile.
 */
extern tileset
tileset_eqclass(tileset ts, const struct puzzle *p)
{
	tileset cmap = tileset_complement(tileset_map(tileset_remove(ts, 0), p));

	if (tileset_has(ts, 0))
		return (tileset_flood(cmap, p->tiles[0]));
	else
		return (cmap);
}

