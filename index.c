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
 * The first INDEX_MAX_TILES factorials.
 */
const unsigned factorials[INDEX_MAX_TILES + 1] = {
	1,
	1,
	2,
	2 * 3,
	2 * 3 * 4,
	2 * 3 * 4 * 5,
	2 * 3 * 4 * 5 * 6,
	2 * 3 * 4 * 5 * 6 * 7,
	2 * 3 * 4 * 5 * 6 * 7 * 8,
	2 * 3 * 4 * 5 * 6 * 7 * 8 * 9,
	2 * 3 * 4 * 5 * 6 * 7 * 8 * 9 * 10,
	2 * 3 * 4 * 5 * 6 * 7 * 8 * 9 * 10 * 11,
	2 * 3 * 4 * 5 * 6 * 7 * 8 * 9 * 10 * 11 * 12,
};

/*
 * Compute the permutation index of those tiles listed in ts which must
 * occupy the grid locations listed in map.  This is done by computing
 * the inversion number of each tile and multiplying them up in a
 * factorial number system.  Each possible permutation of tiles for the
 * same ts and map receives a distinct permutation index between 0 and
 * factorial(tileset_count(ts)) - 1.
 */
static permindex
index_permutation(tileset ts, tileset map, const struct puzzle *p)
{
	permindex factor = 1, n_tiles = tileset_count(ts), pidx;
	unsigned least, leastidx;

	if (tileset_empty(ts))
		return (0);

	/* skip multiplication on first iteration */
	leastidx = tileset_get_least(ts);
	least = p->tiles[tileset_get_least(ts)];
	pidx = tileset_count(tileset_intersect(map, tileset_least(least)));
	map = tileset_remove(map, least);
	ts = tileset_remove_least(ts);

	for (; !tileset_empty(ts); ts = tileset_remove_least(ts)) {
		leastidx = tileset_get_least(ts);
		factor *= n_tiles--;
		least = p->tiles[leastidx];
		pidx += factor * tileset_count(tileset_intersect(map, tileset_least(least)));
		map = tileset_remove(map, least);
	}

	return (pidx);
}

/*
 * Compute the structured index for the equivalence class of p by the
 * tiles selected by ts and store it in idx.  Use index_table to lookup
 * the equivalence class if needed.  If ts does not account for the zero
 * tile, it may be NULL.  See index.h for details on the algorithm.
 */
extern void
compute_index(tileset ts, const struct index_table *idxt,
    struct index *idx, const struct puzzle *p)
{
	tileset map = tileset_map(tileset_remove(ts, ZERO_TILE), p);

	idx->maprank = tileset_rank(map);
	prefetch(idxt + idx->maprank);
	idx->pidx = index_permutation(ts, map, p);

	if (tileset_has(ts, ZERO_TILE))
		idx->eqidx = idxt[idx->maprank].eqclasses[zero_location(p)];
	else
		idx->eqidx = -1; /* mark as invalid */
}

/*
 * Return the grid location in which we want to place the zero tile
 * during decoding.  We make the arbitrary choice of putting it into the
 * lowest numbered tile in the complement map.
 */
static unsigned
canonical_zero_location(tileset ts, const struct index_table *idxt, const struct index *idx)
{
	return (tileset_get_least(eqclass_from_index(ts, idxt, idx)));
}

/*
 * Given a permutation index p, a tileset ts and a tileset map
 * indicating the spots on the grid we want to place the tiles in ts
 * onto, fill in the grid as indicated.
 */
static void
unindex_permutation(struct puzzle *p, tileset ts, tileset map, permindex pidx)
{
	size_t i, cmp;
	permindex n_tiles = tileset_count(ts);
	tileset cmap = tileset_complement(map), tile;

	for (i = 0; i < TILE_COUNT; i++) {
		if (tileset_has(ts, i)) {
			cmp = pidx % n_tiles;
			pidx /= n_tiles--;
			tile = rankselect(map, cmp);
			map &= ~tile;
			p->tiles[i] = tileset_get_least(tile);
		} else {
			p->tiles[i] = tileset_get_least(cmap);
			cmap = tileset_remove_least(cmap);
		}

		p->grid[p->tiles[i]] = i;
	}
}

/*
 * Given a structured index idx for some partial puzzle configuration
 * with tiles selected by ts, compute a representant of the
 * corresponding equivalence class and store it in p.
 */
extern void
invert_index(tileset ts, const struct index_table *idxt, struct puzzle *p, const struct index *idx)
{
	tileset map = tileset_unrank(tileset_count(ts), idx->maprank);

	memset(p, 0, sizeof *p);
	unindex_permutation(p, tileset_remove(ts, ZERO_TILE), map, idx->pidx);

	if (tileset_has(ts, ZERO_TILE))
		move(p, canonical_zero_location(ts, idxt, idx));
}

/*
 * For some purposes, it is useful to be able to combine an index into
 * a single number.  This is done by computing the representation of all
 * components in an appropriate compound base.
 */
extern cmbindex
combine_index(tileset ts, const struct index_table *idxt, const struct index *idx)
{
	cmbindex moffset;

	if (tileset_has(ts, ZERO_TILE))
		moffset = idxt[idx->maprank].offset + idx->eqidx;
	else
		moffset = idx->maprank;

	return (moffset * factorials[tileset_count(ts)] + idx->pidx);
}

/*
 * Split a combined index back up into its bits and pieces.  This
 * operation is quite slow, it's main purpose is to allow
 * pdb_iterate_parallel() to split the PDB into equally-sized pieces.
 */
extern void
split_index(tileset ts, const struct index_table *idxt, struct index *idx, cmbindex cmb)
{
	size_t count = tileset_count(ts), l, m, r;
	cmbindex fac = factorials[count];
	unsigned offset;

	idx->pidx = cmb % fac;
	offset = cmb / fac;

	if (!tileset_has(ts, ZERO_TILE)) {
		idx->maprank = offset;
		idx->eqidx = -1;

		return;
	}

	/* do a binary search through idxt */
	l = 0;
	r = combination_count[count] - 1;
	for (;;) {
		m = l + (r - l) / 2; /* avoid overflow */

		if (idxt[m].offset > offset)
			r = m;
		else if (idxt[m].offset + idxt[m].eqclass_count < offset)
			l = m;
		else {
			idx->pidx = m;
			idx->eqidx = offset - idxt[m].offset;
			return;
		}
	}
}

/*
 * Describe idx as a string and write the result to str.  Only the tiles
 * in ts are printed.
 */
extern void
index_string(tileset ts, char str[INDEX_STR_LEN], const struct index *idx)
{

	(void)ts;

	snprintf(str, INDEX_STR_LEN, "(%u %u %d)\n", idx->pidx, idx->maprank, idx->eqidx);
}
