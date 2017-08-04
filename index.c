/* index.c -- compute puzzle indices */
#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "builtins.h"
#include "tileset.h"
#include "index.h"

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

	for (i = 0; !tileset_empty(ts); ts = tileset_remove_least(ts), i++) {
		least = p->tiles[tileset_get_least(ts)];
		idx->cmp[i] = tileset_count(tileset_intersect(occupation,tileset_least(least)));
		occupation = tileset_remove(occupation, least);
	}

	memset(idx->cmp + i, 0, sizeof idx->cmp - i);
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
