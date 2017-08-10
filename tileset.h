#ifndef TILESET_H
#define TILESET_H

#include "builtins.h"
#include "puzzle.h"

/*
 * For many purposes, it is useful to consider sets of tiles.
 * A tileset represents a set of tiles as a bitmask.
 */
typedef unsigned tileset;

enum {
	EMPTY_TILESET = 0,
	FULL_TILESET = (1 << TILE_COUNT) - 1,

	TILESET_STR_LEN = 2 * 25 + 1,
};

extern tileset	tileset_eqclass(tileset, const struct puzzle *);
extern void	tileset_string(char[TILESET_STR_LEN], tileset);

/*
 * Return 1 if t is in ts.
 */
static inline int
tileset_has(tileset ts, unsigned t)
{
	return ((ts & (tileset)1 << t) != 0);
}

/*
 * Add t to ts and return the updated tileset.
 */
static inline tileset
tileset_add(tileset ts, unsigned t)
{
	return (ts | (tileset)1 << t);
}

/*
 * Remove t from ts and return the updated tileset.
 */
static inline tileset
tileset_remove(tileset ts, unsigned t)
{
	return (ts & ~((tileset)1 << t));
}

/*
 * Return the number of tiles in ts.
 */
static inline unsigned
tileset_count(tileset ts)
{
	return (popcount(ts));
}

/*
 * Return 1 if ts is empty.
 */
static inline int
tileset_empty(tileset ts)
{
	return (ts == 0);
}

/*
 * Return a tileset containing all tiles not in ts.
 */
static inline tileset
tileset_complement(tileset ts)
{
	return (~ts & FULL_TILESET);
}

/*
 * Return a tileset equal to ts without the tile with the lowest number.
 * If ts is empty, return ts unchanged.
 */
static inline tileset
tileset_remove_least(tileset ts)
{
	return (ts & ts - 1);
}

/*
 * Return the number of the lowest numbered tile in ts.  If ts is empty,
 * behaviour is undefined.
 */
static inline unsigned
tileset_get_least(tileset ts)
{
	return (ctz(ts));
}

/*
 * Return the intersection of ts1 and ts2.
 */
static inline tileset
tileset_intersect(tileset ts1, tileset ts2)
{
	return (ts1 & ts2);
}

/*
 * Return a tileset containing the lowest numbered n tiles.
 */
static inline tileset
tileset_least(unsigned n)
{
	return ((1 << n) - 1);
}

/*
 * Given a tileset and a puzzle configuration, compute a tileset
 * representing the squares occupied by tiles in the tileset.
 *
 * TODO: Use SSE4.2 instruction pcmpestrm to compute this quickly.
 */
static inline tileset
tileset_map(tileset ts, const struct puzzle *p)
{
	tileset map = EMPTY_TILESET;

	for (; !tileset_empty(ts); ts = tileset_remove_least(ts))
		map |= 1 << p->tiles[tileset_get_least(ts)];

	return (map);
}

/*
 * Given a tileset eq representing some squares of the grid, return a
 * tileset composed of those squares in eq which are adjacent to a
 * square not in eq.  Intuitively, these are the squares from which a
 * move could possibly lead to a configuration in a different
 * equivalence class.
 */
static inline tileset
tileset_reduce_eqclass(tileset eq)
{
	tileset c = tileset_complement(eq);

	/*
	 * the mask prevents carry into other rows:
	 * 0x0f7bdef: 01111 01111 01111 01111 01111
	 */
	return (eq & (c | c  << 5 | (c & 0x0f7bdef) << 1 | c >> 5 | c >> 1 & 0x0f7bdef));
}


#endif /* TILESET_H */
