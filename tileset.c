/* tileset.c -- dealing with sets of tiles */

#include "puzzle.h"
#include "tileset.h"

/*
 * Given a tileset ts representing free positions on the grid and
 * a square number t set in ts, return a tileset representing
 * all squares reachable from t through members of ts.
 */
static tileset
tileset_flood(tileset ts, unsigned t)
{
	tileset r = tileset_add(EMPTY_TILESET, t), oldr;

	do {
		oldr = r;
		r = ts & (r | r  << 5 | r << 1 | r >> 5 | r >> 1);
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
 * the non-zero tiles in ts.
 */
extern tileset
tileset_eqclass(tileset ts, const struct puzzle *p)
{
	return (tileset_flood(tileset_map(tileset_remove(ts, 0), p), p->tiles[0]));
}

