#ifndef TILESET_H
#define TILESET_H

#include <stdint.h>

#include "builtins.h"

/*
 * For many purposes, it is useful to consider sets of tiles.
 * A tileset represents a set of tiles as a bitmask.
 */
typedef uint_least32_t tileset;

enum {
	EMPTY_TILESET = 0,
	FULL_TILESET = 0x1ffffff,
};

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

#endif /* TILESET_H */
