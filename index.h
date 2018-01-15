/*-
 * Copyright (c) 2017--2018 Robert Clausecker. All rights reserved.
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

#ifndef INDEX_H
#define INDEX_H

#ifdef __SSE__
# include <immintrin.h>
#endif

#include <stdalign.h>
#include <stdatomic.h>

#include "puzzle.h"
#include "tileset.h"

/*
 * To build pattern databases, we need a perfect bijective hash function
 * from partial puzzle configurations to integers in 0 ... n-1 where n
 * is the number of possible partial puzzle configurations for the given
 * tile set.  In this project, we compute the index as follows:
 *
 * 1. A map of which positions on the grid are occupied by tiles in ts
 *    (the set of tiles we consider) is created
 * 2. the combinatorial rank of this map is computed and stored in maprank
 * 3. the grid is compressed to only those spots in the map.  What is
 *    left is an array comprising a permutation of the tiles we are
 *    interested in
 * 4. an inversion vector for this array is computed and multiplied into
 *    a permutation index pidx.
 * 5. the index of the equivalence class the location of the zero tile
 *    belongs to is stored in eqclass if the zero tile is accounted for.
 *
 * This gives us an index in three parts which can easily be turned into
 * a single number if needed.  However, for many use cases, we are
 * content with having the index split up into its components, which is
 * why we separate the computation of the structured index (struct
 * index) and the index product (index).
 */

/* good enough unless INDEX_MAX_TILES > 12 */
typedef unsigned permindex;
struct index {
	permindex pidx;
	tsrank maprank;
	int eqidx;
};

/*
 * For the indexing and unindexing operations we use this auxillary
 * structure.  It contains everything we need to quickly compute and
 * reverse tilesets for a given tile set.  It also contains a lookup
 * table containing for each possible map an array eqclasses assigning
 * to each empty grid position the index of its equivalence class (-1
 * for occupied grid positions) and a member offset that contains the
 * sum of number of equivalence classes in all preceding array entries,
 * i.e. the index the first equivalence class in this array would have
 * when all equivalence classes for all possible maps for a given
 * tileset are stored sequentially.  Note that this auxillary structure
 * is valid for all tilesets with the same amount of tiles and can thus
 * be shared between threads.
 */
struct index_table {
	signed char eqclasses[TILE_COUNT];
	unsigned char n_eqclass;
	unsigned offset;
};

/*
 * For the indexing and unindexing operations we use this auxillary
 * structure.  It contains everything we need to quickly compute and
 * reverse tilesets for a given tile set, including a pointer to an
 * appropriate strzct index_table.
 */
struct index_aux {
	alignas(32) unsigned char tsmask[32]; /* for use with SSE 4.2 and AVX2 puzzle_partially_equal() */
	alignas(16) unsigned char tiles[16]; /* for use with the SSE 4.2 tileset_map() */

	unsigned n_tile; /* number of tiles not including the zero tile */
	unsigned n_maprank; /* number of different maprank values */
	unsigned n_perm; /* number of permutations */
	unsigned solved_parity; /* parity of the solved configuration */

	tileset ts;
	struct index_table *idxt;
};

enum {
	/* maximal number of nonzero tiles in partial index */
	INDEX_MAX_TILES = 12,

	/* buffer length for index_string() */
	INDEX_STR_LEN = 27, /* (########## ########## ##)\0 */
};

extern void	compute_index(const struct index_aux*, struct index*, const struct puzzle*);
extern void	invert_index(const struct index_aux*, struct puzzle*, const struct index*);
extern void	invert_index_map(const struct index_aux*, struct puzzle*, const struct index*);
extern void	invert_index_rest(const struct index_aux*, struct puzzle*, const struct index*);
extern void	index_string(tileset, char[INDEX_STR_LEN], const struct index*);
extern void	make_index_aux(struct index_aux*, tileset);
extern int	puzzle_partially_equal(const struct puzzle *, const struct puzzle *, const struct index_aux *);


extern const unsigned factorials[INDEX_MAX_TILES + 1];

/*
 * Given an index_aux structure and a maprank within that index, return
 * the number of equivalence classes for that map.  If the zero tile is
 * not accounted for, this number will be 1.
 */
static inline unsigned
eqclass_count(const struct index_aux *aux, tsrank maprank)
{
	if (tileset_has(aux->ts, ZERO_TILE))
		return (aux->idxt[maprank].n_eqclass);
	else
		return (1);
}

/*
 * Return the total number of equivalence class for all possible maps of
 * the index described by aux.
 */
static inline unsigned
eqclass_total(const struct index_aux *aux)
{
	if (tileset_has(aux->ts, ZERO_TILE))
		return (aux->idxt[aux->n_maprank - 1].offset + aux->idxt[aux->n_maprank - 1].n_eqclass);
	else
		return (aux->n_maprank);
}

/*
 * Compute the number of possible values of an index in aux.
 * This is one higher than the highest index combine_index() would
 * generate for an index in ts.
 */
static inline size_t
search_space_size(const struct index_aux *aux)
{
	return ((size_t)aux->n_perm * (size_t)eqclass_total(aux));
}

/*
 * Compute the offset a configuration for index idx would have from the
 * beginning of the PDB if each PDB entry was one byte in size.
 */
static inline size_t
index_offset(const struct index_aux *aux, const struct index *idx)
{
	size_t map_offset;

	if (tileset_has(aux->ts, ZERO_TILE))
		map_offset = aux->idxt[idx->maprank].offset + idx->eqidx;
	else
		map_offset = idx->maprank;

	return (map_offset * aux->n_perm + idx->pidx);
}

/*
 * Given a permutation index, compute the corresponding equivalence
 * class map by forming a map from the appropriate entry in idxt and
 * return it.  If the zero tile is not accounted for, instead return
 * a map of all grid spots occupied.  idxt may be NULL in this case.
 */
static inline tileset
eqclass_from_index(const struct index_aux *aux, const struct index *idx)
{
	if (!tileset_has(aux->ts, ZERO_TILE))
		return (tileset_complement(tileset_unrank(tileset_count(aux->ts), idx->maprank)));

#ifdef __AVX2__
	/* load overshoots eqclasses, should be fine */
	__m256i eqidx = _mm256_set1_epi8(idx->eqidx);
	__m256i map = _mm256_loadu_si256((const __m256i*)aux->idxt[idx->maprank].eqclasses);

	map = _mm256_cmpeq_epi8(map, eqidx);

	return (_mm256_movemask_epi8(map) & FULL_TILESET);
#elif defined(__SSE2__)
	/* the second load overshoots eqclasses, should be fine, too */
	__m128i eqidx = _mm_set1_epi8(idx->eqidx);
	__m128i lo = _mm_loadu_si128((const __m128i*)aux->idxt[idx->maprank].eqclasses + 0);
	__m128i hi = _mm_loadu_si128((const __m128i*)aux->idxt[idx->maprank].eqclasses + 1);
	tileset eq;

	lo = _mm_cmpeq_epi8(lo, eqidx);
	hi = _mm_cmpeq_epi8(hi, eqidx);

	eq = _mm_movemask_epi8(lo) | _mm_movemask_epi8(hi) << 16;
	return (eq & FULL_TILESET);
#else /* no SSE2, no AVX2 */
	size_t i;
	tileset eq = EMPTY_TILESET;

	for (i = 0; i < TILE_COUNT; i++)
		if (aux->idxt[idx->maprank].eqclasses[i] == idx->eqidx)
			eq = tileset_add(eq, i);

	return (eq);
#endif /* __AVX2__ */
}

/*
 * Return a tileset specifying which grid locations in p are occupied by
 * nonzero tiles in aux->ts.
 */
static inline tileset
tile_map(const struct index_aux *aux, const struct puzzle *p)
{
#ifdef __AVX2__
	/* load complemented tiles */
	__m128i tiles = _mm_loadu_si128((const __m128i*)aux->tiles);

	/* load grid and complement to circumvent pcmpistri's string termination check */
	__m256i grid = _mm256_andnot_si256(_mm256_loadu_si256((const __m256i*)p->grid),
	    _mm256_set_epi64x(0xffull, -1ull, -1ull, -1ull));

	/* compute the bitmasks */
#define OPERATION (_SIDD_UBYTE_OPS|_SIDD_CMP_EQUAL_ANY|_SIDD_BIT_MASK)
	__m128i maplo = _mm_cmpistrm(tiles, _mm256_castsi256_si128(grid), OPERATION);
	__m128i maphi = _mm_cmpistrm(tiles, _mm256_extracti128_si256(grid, 1), OPERATION);
	maplo = _mm_unpacklo_epi16(maplo, maphi);
#undef OPERATION

	return (_mm_cvtsi128_si32(maplo));
#elif defined(__SSE4_2__)
	/*
	 * this code is very similar to the AVX code except for the more
	 * complex masking in the beginning due to the lack of 256 bit
	 * registers.
	 */

	/* load complemented tiles */
	__m128i tiles = _mm_loadu_si128((const __m128i*)aux->tiles);

	/* load grid and complement to circumvent pcmpistri's string termination check */
	__m128i gridmask = _mm_set1_epi8(0xff);
	__m128i gridlo = _mm_andnot_si128(_mm_loadu_si128((const __m128i*)p->grid + 0), gridmask);

	/* compute the bitmasks */
#define OPERATION (_SIDD_UBYTE_OPS|_SIDD_CMP_EQUAL_ANY|_SIDD_BIT_MASK)
	__m128i maplo = _mm_cmpistrm(tiles, gridlo, OPERATION);
	__m128i gridhi = _mm_andnot_si128(_mm_loadu_si128((const __m128i*)p->grid + 1), _mm_bsrli_si128(gridmask, 7));
	__m128i maphi = _mm_cmpistrm(tiles, gridhi, OPERATION);
	maplo = _mm_unpacklo_epi16(maplo, maphi);
#undef OPERATION

	return (_mm_cvtsi128_si32(maplo));
#else
	tileset tsnz = tileset_remove(aux->ts, ZERO_TILE), map = EMPTY_TILESET;

	for (; !tileset_empty(tsnz); tsnz = tileset_remove_least(tsnz))
		map |= 1 << p->tiles[tileset_get_least(tsnz)];

	return (map);
#endif
}

#endif /* INDEX_H */
