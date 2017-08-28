#ifndef INDEX_H
#define INDEX_H

#ifdef __SSE__
# include <immintrin.h>
#endif

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
	unsigned offset;
	signed char eqclasses[TILE_COUNT];
	unsigned char n_eqclass;
};

/*
 * For the indexing and unindexing operations we use this auxillary
 * structure.  It contains everything we need to quickly compute and
 * reverse tilesets for a given tile set, including a pointer to an
 * appropriate strzct index_table.
 */
struct index_aux {
	tileset ts;
	struct index_table *idxt;
};

/*
 * This type represents all the components of struct index combined into
 * a single number.
 */
typedef unsigned long long cmbindex;

enum {
	/* maximal number of nonzero tiles in partial index */
	INDEX_MAX_TILES = 12,

	/* buffer length for index_string() */
	INDEX_STR_LEN = 28, /* (########## ########## ##)\n\0 */
};

extern void	compute_index(const struct index_aux*, struct index*, const struct puzzle*);
extern void	invert_index(const struct index_aux*, struct puzzle*, const struct index*);
extern cmbindex	combine_index(const struct index_aux*, const struct index*);
extern void	split_index(const struct index_aux*, struct index*, cmbindex);
extern void	index_string(tileset, char[INDEX_STR_LEN], const struct index*);
extern void	make_index_aux(struct index_aux*, tileset);

extern const unsigned factorials[INDEX_MAX_TILES + 1];

/*
 * Compute the number of possible values of an index for tile set ts.
 * This is one higher than the highest index combine_index() would
 * generate for an index in ts.
 */
static inline cmbindex
search_space_size(tileset ts, const struct index_table *idxt)
{
	size_t tscount = tileset_count(ts), ccount = combination_count[tscount];

	if (tileset_has(ts, ZERO_TILE))
		return ((idxt[ccount - 1].offset + idxt[ccount - 1].n_eqclass) * factorials[tscount]);
	else
		return (ccount * factorials[tscount]);
}

/*
 * Given a permutation index, compute the corresponding equivalence
 * class map by forming a map from the appropriate entry in idxt and
 * return it.  If the zero tile is not accounted for, instead return
 * a map of all grid spots zboccupied.  idxt may be NULL in this case.
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

/* random.c */
extern unsigned random_seed;
extern void	random_index(const struct index_aux *, struct index *);

#endif /* INDEX_H */
