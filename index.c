/* index.c -- compute puzzle indices */
#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "builtins.h"
#include "tileset.h"
#include "index.h"
#include "puzzle.h"

/*
 * Compute the index for tiles ts in p and store them in idx.
 * It is assumed that p is a valid puzzle.  The remaining index
 * entries are set to 0 by this function.
 */
extern void
compute_index(tileset ts, struct index *idx, const struct puzzle *p)
{
	size_t i, least;
	tileset occupation = FULL_TILESET;

	memset(idx->cmp, 0, TILE_COUNT);

	for (i = 0; !tileset_empty(ts); ts = tileset_remove_least(ts), i++) {
		least = p->tiles[tileset_get_least(ts)];
		idx->cmp[i] = tileset_count(tileset_intersect(occupation,tileset_least(least)));
		occupation = tileset_remove(occupation, least);
	}
}

/*
 * Compute the tile configuration from a structured index.  If the
 * index contains less pieces than it should, the remaining pieces are
 * assigned in an unpredictable manner.  This function assumes that
 * the trailing unused fields in idx are set to zero as compute_index()
 * does anyway.
 */
extern void
invert_index(tileset ts, struct puzzle *p, const struct index *idx)
{
	/* in base 32 this is 24 23 22 ... 2 1 0 */
	__int128 occupation = (__int128)1782769360921721754ULL << 64 | 14242959524133701664ULL, mask;
	size_t i;

	/* unneeded, kept as an argument to simplify future changes */
	(void)ts;

	for (i = 0; i < TILE_COUNT; i++) {
		mask = ((__int128)1 << 5 * idx->cmp[i]) - 1;
		p->tiles[i] = 31 & occupation >> 5 * idx->cmp[i];
		p->grid[p->tiles[i]] = i;
		occupation = (occupation & mask) | (occupation >> 5 & ~mask);
	}
}

/*
 * This table contains partial products 25, 25 * 24, 25 * 24 * 23, ...
 * for use by combine_index.  To keep all numbers inside 32 bits, we
 * omit the terms 25 * 24 * ... * 19 in the second half.  This allows us
 * to use mostly 32 bit multiplications which are a little bit faster on
 * some architectures.
 */
static const unsigned
partial_products[16] = {
	1u,
	25u,
	25u * 24,
	25u * 24 * 23,
	25u * 24 * 23 * 22,
	25u * 24 * 23 * 22 * 21,
	25u * 24 * 23 * 22 * 21 * 20,
	25u * 24 * 23 * 22 * 21 * 20 * 19,

	18u,
	18u * 17,
	18u * 17 * 16,
	18u * 17 * 16 * 15,
	18u * 17 * 16 * 15 * 14,
	18u * 17 * 16 * 15 * 14 * 13,
	18u * 17 * 16 * 15 * 14 * 13 * 12,
	18u * 17 * 16 * 15 * 14 * 13 * 12 * 11,
};

/*
 * Combine index idx for tileset ts into a single number.  It is
 * assumed that ts contains no more than 16 members as otherwise, an
 * overflow could occurs.
 *
 * TODO: Find optimal permutation of index components.
 */
extern cmbindex
combine_index(tileset ts, const struct index *idx)
{
	size_t i, n = tileset_count(ts);
	unsigned accum1 = 0, accum2 = 0;

	assert(n <= 16);

	for (i = 0; i < 8; i++)
		accum1 += partial_products[i] * idx->cmp[i];

	if (n > 8)
		for (i = 8; i < 16; i++)
			accum2 += partial_products[i] * idx->cmp[i];

	return (accum1 + accum2 * (25ULL * 23 * 22 * 21 * 20 * 19));
}
