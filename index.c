/* index.c -- compute puzzle indices */
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
 * This table stores pointers to the index_table structures generated
 * by make_index_table so we only generate one table for each tile set
 * size.
 */
struct index_table *index_tables[INDEX_MAX_TILES + 1] = {};

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
 * tiles selected by aux->ts and store it in idx.  Use aux to lookup
 * other bits and pieces if needed.  See index.h for details on the
 * algorithm.
 */
extern void
compute_index(const struct index_aux *aux, struct index *idx, const struct puzzle *p)
{
	tileset tsnz = tileset_remove(aux->ts, ZERO_TILE), map = tileset_map(tsnz, p);

	idx->maprank = tileset_rank(map);
	prefetch(aux->idxt + idx->maprank);
	idx->pidx = index_permutation(tsnz, map, p);

	if (tileset_has(aux->ts, ZERO_TILE))
		idx->eqidx = aux->idxt[idx->maprank].eqclasses[zero_location(p)];
	else
		idx->eqidx = -1; /* mark as invalid */
}

/*
 * Return the grid location in which we want to place the zero tile
 * during decoding.  We make the arbitrary choice of putting it into the
 * lowest numbered tile in the equivalence class.
 */
static unsigned
canonical_zero_location(const struct index_aux *aux, const struct index *idx)
{
	return (tileset_get_least(eqclass_from_index(aux, idx)));
}

/*
 * Given a permutation index p, a tileset ts and a tileset map
 * indicating the spots on the grid we want to place the tiles in ts
 * onto, fill in the grid as indicated by pidx.
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
			map = tileset_difference(map, tile);
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
invert_index(const struct index_aux *aux, struct puzzle *p, const struct index *idx)
{
	tileset tsnz = tileset_remove(aux->ts, ZERO_TILE);
	tileset map = tileset_unrank(tileset_count(tsnz), idx->maprank);

	memset(p, 0, sizeof *p);
	prefetch(aux->idxt + idx->maprank);
	unindex_permutation(p, tsnz, map, idx->pidx);

	if (tileset_has(aux->ts, ZERO_TILE))
		move(p, canonical_zero_location(aux, idx));
}

/*
 * For some purposes, it is useful to be able to combine an index into
 * a single number.  This is done by computing the representation of all
 * components in an appropriate compound base.
 */
extern cmbindex
combine_index(const struct index_aux *aux, const struct index *idx)
{
	cmbindex moffset;

	if (tileset_has(aux->ts, ZERO_TILE))
		moffset = aux->idxt[idx->maprank].offset + idx->eqidx;
	else
		moffset = idx->maprank;

	return (moffset * aux->n_perm + idx->pidx);
}

/*
 * Split a combined index back up into its bits and pieces.  This
 * operation is quite slow, it's main purpose is to allow
 * pdb_iterate_parallel() to split the PDB into equally-sized pieces.
 */
extern void
split_index(const struct index_aux *aux, struct index *idx, cmbindex cmb)
{
	size_t l, m, r;
	unsigned offset;

	idx->pidx = cmb % aux->n_perm;
	offset = cmb / aux->n_perm;

	if (!tileset_has(aux->ts, ZERO_TILE)) {
		idx->maprank = offset;
		idx->eqidx = -1;

		return;
	}

	/* do a binary search through idxt */
	for (l = 0, r = aux->n_maprank; r != 0; r >>= 1) {
		m = l + (r >> 1);

		if (offset >= aux->idxt[m].offset + aux->idxt[m].n_eqclass) {
			l = m + 1;
			r--;
		} else if (offset >= aux->idxt[m].offset) {
			idx->maprank = m;
			idx->eqidx = offset - aux->idxt[m].offset;
			return;
		} /* else move left */
	}
}

/*
 * Allocate and initialize the lookup table for index generation for
 * tileset ts.  If storage is insufficient, abort the program.  If ts
 * does not account for the zero tile, return NULL.
 */
static struct index_table *
make_index_table(tileset ts)
{
	struct index_table *idxt;
	size_t i, n, tscount;
	tileset map;
	unsigned offset = 0;

	if (!tileset_has(ts, ZERO_TILE))
		return (NULL);

	ts = tileset_remove(ts, ZERO_TILE);
	tscount = tileset_count(ts);
	if (index_tables[tscount] != NULL)
		return (index_tables[tscount]);

	n = combination_count[tscount];
	idxt = malloc(n * sizeof *idxt);
	if (idxt == NULL) {
		perror("malloc");
		abort();
	}

	map = tileset_least(tscount);
	for (i = 0; i < n; i++) {
		idxt[i].offset = offset;
		idxt[i].n_eqclass = tileset_populate_eqclasses(idxt[i].eqclasses, map);
		offset += idxt[i].n_eqclass;
		map = next_combination(map);
	}

	index_tables[tscount] = idxt;
	return (idxt);
}

/*
 * Initialize aux with the correct values to compute indices for the
 * tileset ts.  Allocate tables as needed.  If storage is insufficient
 * for the required tables, abort the program.
 */
extern void
make_index_aux(struct index_aux *aux, tileset ts)
{
	aux->ts = ts;
	aux->n_tile = tileset_count(tileset_remove(ts, ZERO_TILE));
	aux->n_maprank = combination_count[aux->n_tile];
	aux->n_perm = factorials[aux->n_tile];
	aux->idxt = make_index_table(ts);
}

/*
 * Describe idx as a string and write the result to str.  Only the tiles
 * in ts are printed.
 */
extern void
index_string(tileset ts, char str[INDEX_STR_LEN], const struct index *idx)
{

	(void)ts;

	snprintf(str, INDEX_STR_LEN, "(%u %u %d)", idx->pidx, idx->maprank, idx->eqidx);
}
