/* index.c -- compute puzzle indices */
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "builtins.h"
#include "tileset.h"
#include "index.h"
#include "puzzle.h"

/*
 * Search space sizes for the first 16 tablebase sizes.
 */
const cmbindex search_space_sizes[16] = {
	1LLU,
	25LLU,
	25LLU * 24,
	25LLU * 24 * 23,
	25LLU * 24 * 23 * 22,
	25LLU * 24 * 23 * 22 * 21,
	25LLU * 24 * 23 * 22 * 21 * 20,
	25LLU * 24 * 23 * 22 * 21 * 20 * 19,
	25LLU * 24 * 23 * 22 * 21 * 20 * 19 * 18,
	25LLU * 24 * 23 * 22 * 21 * 20 * 19 * 18 * 17,
	25LLU * 24 * 23 * 22 * 21 * 20 * 19 * 18 * 17 * 16,
	25LLU * 24 * 23 * 22 * 21 * 20 * 19 * 18 * 17 * 16 * 15,
	25LLU * 24 * 23 * 22 * 21 * 20 * 19 * 18 * 17 * 16 * 15 * 14,
	25LLU * 24 * 23 * 22 * 21 * 20 * 19 * 18 * 17 * 16 * 15 * 14 * 13,
	25LLU * 24 * 23 * 22 * 21 * 20 * 19 * 18 * 17 * 16 * 15 * 14 * 13 * 12,
	25LLU * 24 * 23 * 22 * 21 * 20 * 19 * 18 * 17 * 16 * 15 * 14 * 13 * 12 * 11,
};

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

	memset(idx, 0, sizeof *idx);

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
 * does anyway.  If ts is not FULL_TILESET, a representant of the
 * class of configurations indexed by idx and ts is chosen arbitrarily.
 * This implementation always chooses the least representant.
 */
extern void
invert_index(tileset ts, struct puzzle *p, const struct index *idx)
{
	/* in base 32 this is 24 23 22 ... 2 1 0 */
	__int128 occupation = (__int128)1782769360921721754ULL << 64 | 14242959524133701664ULL, mask;
	size_t i, least;
	tileset tsc = tileset_complement(ts);

	/* clear padding */
	memset(p, 0, sizeof *p);

	/* fill tiles in idx, ts */
	for (i = 0; !tileset_empty(ts); ts = tileset_remove_least(ts), i++) {
		least = tileset_get_least(ts);
		mask = ((__int128)1 << 5 * idx->cmp[i]) - 1;
		p->tiles[least] = 31 & occupation >> 5 * idx->cmp[i];
		p->grid[p->tiles[least]] = least;
		occupation = (occupation & mask) | (occupation >> 5 & ~mask);
	}

	/* fill remaining tiles as if encoded as 0 */
	for (; !tileset_empty(tsc); tsc = tileset_remove_least(tsc)) {
		least = tileset_get_least(tsc);
		p->tiles[least] = 31 & occupation;
		p->grid[p->tiles[least]] = least;
		occupation >>= 5;
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
partial_products[15] = {
	1u,
	25u,
	25u * 24,
	25u * 24 * 23,
	25u * 24 * 23 * 22,
	25u * 24 * 23 * 22 * 21,
	25u * 24 * 23 * 22 * 21 * 20,

	1u,
	18u,
	18u * 17,
	18u * 17 * 16,
	18u * 17 * 16 * 15,
	18u * 17 * 16 * 15 * 14,
	18u * 17 * 16 * 15 * 14 * 13,
	18u * 17 * 16 * 15 * 14 * 13 * 12,
};

/*
 * Perform compute_index() and combine_index() in one step to speed up
 * the index computation.
 */
extern cmbindex
full_index(tileset ts, const struct puzzle *p)
{
	size_t least;
	cmbindex accum = 0, factor = 1, i;
	tileset occupation = FULL_TILESET;

	for (i = 25; !tileset_empty(ts); ts = tileset_remove_least(ts), i--) {
		least = p->tiles[tileset_get_least(ts)];
		accum += factor * tileset_count(tileset_intersect(occupation,tileset_least(least)));
		factor *= i;
		occupation = tileset_remove(occupation, least);
	}

	return (accum);
}

/*
 * Combine index idx for tileset ts into a single number.  It is
 * assumed that ts contains no more than 15 members as otherwise, an
 * overflow could occur.
 *
 * TODO: Find optimal permutation of index components.
 */
extern cmbindex
combine_index(tileset ts, const struct index *idx)
{
	size_t i, n = tileset_count(ts);
	cmbindex accum1 = 0, accum2 = 0;

	for (i = 0; i < 7; i++)
		accum1 += partial_products[i] * idx->cmp[i];

	if (n > 7)
		for (i = 7; i < 15; i++)
			accum2 += partial_products[i] * idx->cmp[i];

	return (accum1 + accum2 * (25ULL * 24 * 23 * 22 * 21 * 20 * 19));
}

/*
 * Split a combined index into a separate index.  As with the function
 * compute_index(), the unused parts of idx are filled with zeros.  This
 * function is very slow sadly.
 */
extern void
split_index(tileset ts, struct index *idx, cmbindex cmb)
{
	size_t i, n = tileset_count(ts);

	memset(idx, 0, sizeof *idx);

	for (i = 0; i < n; i++) {
		idx->cmp[i] = cmb % (25 - i);
		cmb /= 25 - i;
	}
}

/*
 * Describe idx as a string and write the result to str.  Only the tiles
 * in ts are printed.
 */
extern void
index_string(tileset ts, char str[INDEX_STR_LEN], const struct index *idx)
{
	size_t i, j, n = tileset_count(ts);

	for (i = j = 0; i < TILE_COUNT; i++)
		if (i < n)
			sprintf(str + 3 * i, "%2d ", idx->cmp[j++]);
		else
			strcpy(str + 3 * i, "   ");

	str[3 * TILE_COUNT] = '\n';
}
