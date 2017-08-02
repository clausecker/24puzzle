#ifndef TILESET_H
#define TILESET_H

#include <stddef.h>
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
inline int
tileset_has(tileset ts, size_t t)
{
	return ((ts & (tileset)1 << t) != 0);
}

/*
 * Add t to ts and return the updated tileset.
 */
inline tileset
tileset_add(tileset ts, size_t t)
{
	return (ts | (tileset)1 << t);
}

/*
 * Remove t from ts and return the updated tileset.
 */
inline tileset
tileset_remove(tileset ts, size_t t)
{
	return (ts & ~((tileset)1 << t));
}

/*
 * Return the number of tiles in ts.
 */
inline int
tileset_count(tileset ts)
{
	return (popcount(ts));
}

#endif /* TILESET_H */
