/*-
 * Copyright (c) 2017 Robert Clausecker. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* index.c -- compute puzzle indices */

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __SSE__
# include <immintrin.h>
#endif

#ifdef __SSE4__
# include <nmmintrin.h>
#endif

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
	tileset tsnz = tileset_remove(aux->ts, ZERO_TILE), map = tile_map(aux, p);

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
 * Given a tileset ts and a map m, fill in all tiles not in ts into the
 * spots not on m.
 */
static void
fill_cmap(struct puzzle *p, tileset ts, tileset map)
{
	size_t i;
	tileset cmap = tileset_complement(map), cts;

	for (cts = tileset_complement(ts); !tileset_empty(cts); cts = tileset_remove_least(cts)) {
		i = tileset_get_least(cts);
		p->tiles[i] = tileset_get_least(cmap);
		cmap = tileset_remove_least(cmap);
		p->grid[p->tiles[i]] = i;
	}

}

/*
 * Given a permutation index p, a tileset ts and a tileset map
 * indicating the spots on the grid we want to place the tiles in ts
 * onto, fill in the grid as indicated by pidx.
 */
static void
unindex_permutation(struct puzzle *p, tileset ts, tileset map, permindex pidx)
{
	size_t i;
	permindex cmp, n_tiles;
	tileset tile;

	for (n_tiles = tileset_count(ts); n_tiles > 0; n_tiles--) {
		cmp = pidx % n_tiles;
		pidx /= n_tiles;
		i = tileset_get_least(ts);
		ts = tileset_remove_least(ts);
		tile = rankselect(map, cmp);
		p->tiles[i] = tileset_get_least(tile);
		map = tileset_difference(map, tile);
		p->grid[p->tiles[i]] = i;
	}
}

/*
 * Half of the work of inverting an index depends on the map only.  This
 * function does this first part only to speed up index inversion for
 * multiple indices in the same map which can re-use the partially
 * generated index returned by this function.
 */
extern void
invert_index_map(const struct index_aux *aux, struct puzzle *p, const struct index *idx)
{
	tileset tsnz = tileset_remove(aux->ts, ZERO_TILE);
	tileset map = tileset_unrank(tileset_count(tsnz), idx->maprank);

	memset(p, 0, sizeof *p);
	fill_cmap(p, tsnz, map);
}

/*
 * This function does the other half of the work begun by
 * invert_index_map().  p must be the result of invert_index_map() for
 * an index within the same cohort as idx, but it is okay to arbitrarily
 * permute tiles not in aux->ts including the zero tile.
 */
extern void
invert_index_rest(const struct index_aux *aux, struct puzzle *p, const struct index *idx)
{
	tileset tsnz = tileset_remove(aux->ts, ZERO_TILE);
	tileset map = tileset_unrank(tileset_count(tsnz), idx->maprank);

	prefetch(aux->idxt + idx->maprank);
	unindex_permutation(p, tsnz, map, idx->pidx);

	if (tileset_has(aux->ts, ZERO_TILE))
		move(p, canonical_zero_location(aux, idx));
}

/*
 * Given a structured index idx for some partial puzzle configuration
 * with tiles selected by ts, compute a representant of the
 * corresponding equivalence class and store it in p.
 */
extern void
invert_index(const struct index_aux *aux, struct puzzle *p, const struct index *idx)
{
	invert_index_map(aux, p, idx);
	invert_index_rest(aux, p, idx);
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
	tileset tsnz = tileset_remove(ts, ZERO_TILE);
	size_t i = 0;

	aux->ts = ts;
	aux->n_tile = tileset_count(tsnz);
	aux->n_maprank = combination_count[aux->n_tile];
	aux->n_perm = factorials[aux->n_tile];

	tileset_unrank_init(aux->n_tile);

	/* see tileset_map() for details */
	memset(aux->tiles, 0, sizeof aux->tiles);
	for (; !tileset_empty(tsnz); tsnz = tileset_remove_least(tsnz))
		aux->tiles[i++] = ~tileset_get_least(tsnz);

	/* see puzzle_partially_equal() for details */
	for (i = 0; i < sizeof aux->tsmask; i++)
		aux->tsmask[i] = tsnz & 1 << i ? -1 : 0;

	aux->solved_parity = tileset_parity(tsnz);
	aux->idxt = make_index_table(aux->ts);
}

/*
 * Check if puzzle configurations a and b are equal with respect to the
 * tiles specified in aux->ts.  Return nonzero if they are, zero
 * otherwise.
 */
extern int
puzzle_partially_equal(const struct puzzle *a, const struct puzzle *b,
    const struct index_aux *aux)
{
	const signed char *eqclasses;

#ifdef __AVX2__
	__m256i atiles = _mm256_loadu_si256((const __m256i*)a->tiles);
	__m256i btiles = _mm256_loadu_si256((const __m256i*)b->tiles);
	__m256i tsmask = _mm256_loadu_si256((const __m256i*)aux->tsmask);

	if (!_mm256_testc_si256(_mm256_cmpeq_epi8(atiles, btiles), tsmask))
		return (0);
#elif defined(__SSE4_1__)
	/* same algorithm as the AVX2 version, but with 128 bit registers */

	__m128i atileslo = _mm_loadu_si128((const __m128i*)a->tiles + 0);
	__m128i atileshi = _mm_loadu_si128((const __m128i*)a->tiles + 1);
	__m128i btileslo = _mm_loadu_si128((const __m128i*)b->tiles + 0);
	__m128i btileshi = _mm_loadu_si128((const __m128i*)b->tiles + 1);
	__m128i tsmasklo = _mm_loadu_si128((const __m128i*)aux->tsmask + 0);
	__m128i tsmaskhi = _mm_loadu_si128((const __m128i*)aux->tsmask + 1);

	__m128i uneqlo = _mm_andnot_si128(_mm_cmpeq_epi8(atileslo, btileslo), tsmasklo);
	__m128i uneqhi = _mm_andnot_si128(_mm_cmpeq_epi8(atileshi, btileshi), tsmaskhi);
	__m128i uneq = _mm_or_si128(uneqlo, uneqhi);

	if (!_mm_testz_si128(uneq, uneq))
		return (0);
#else
	size_t i;
	tileset tsnz = tileset_remove(aux->ts, ZERO_TILE);

	for (; !tileset_empty(tsnz); tsnz = tileset_remove_least(tsnz)) {
		i = tileset_get_least(tsnz);
		if (a->tiles[i] != b->tiles[i])
			return (0);
	}
#endif
	if (!tileset_has(aux->ts, ZERO_TILE))
		return (1);

	/*
	 * if we care about the zero tile, make sure both puzzles
	 * have the same zero tile region.
	 */
	eqclasses = aux->idxt[tileset_rank(tile_map(aux, a))].eqclasses;

	return (eqclasses[zero_location(a)] == eqclasses[zero_location(b)]);
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
