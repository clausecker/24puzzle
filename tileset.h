#ifndef TILESET_H
#define TILESET_H

#ifdef __SSE__
# include <immintrin.h>
#endif

#ifdef __SSE4__
# include <nmmintrin.h>
#endif

#include "builtins.h"
#include "puzzle.h"

/*
 * For many purposes, it is useful to consider sets of tiles.
 * A tileset represents a set of tiles as a bitmask.
 */
typedef unsigned tileset;

enum {
	EMPTY_TILESET = 0,
	FULL_TILESET = (1 << TILE_COUNT) - 1,

	TILESET_STR_LEN = 2 * 25 + 1,
};

extern tileset	tileset_eqclass(tileset, const struct puzzle *);
extern void	tileset_string(char[TILESET_STR_LEN], tileset);
extern int	tileset_parse(tileset *, const char *);
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
 * Return a tileset containing all tiles not in ts.
 */
static inline tileset
tileset_complement(tileset ts)
{
	return (~ts & FULL_TILESET);
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

/*
 * Given a tileset and a puzzle configuration, compute a tileset
 * representing the squares occupied by tiles in the tileset.
 *
 * TODO: Use SSE4.2 instruction pcmpestrm to compute this quickly.
 */
static inline tileset
tileset_map(tileset ts, const struct puzzle *p)
{

#ifdef __AVX2__
	/*
	 * first, create a mask of those spots we are not interested in.
	 * We mark spots beyond the end of the tiles array as "interesting"
	 * under the assumption that they are zero.  This allows us to use
	 * pcmpistrm instead of the more expensive pcmpestrm.
	 */
	__m256i shufmask = _mm256_set_epi64x(0x0303030303030303ULL,0x0202020202020202ULL,
	    0x0101010101010101,0x0000000000000000ULL);
	__m256i mask = _mm256_set1_epi32(tileset_complement(ts));
	__m256i bitmask = _mm256_set1_epi64x(0x8040201008040201ULL);
	mask = _mm256_shuffle_epi8(mask, shufmask);
	mask = _mm256_cmpeq_epi8(_mm256_and_si256(mask, bitmask), bitmask);

	/* create sets of grid positions from the masks */
	__m256i tilemsk = _mm256_set_epi64x(0x0000000000000098ULL, 0x9796959493929190ULL,
	    0x8f8e8d8c8b8a8988ULL, 0x8786858483828180ULL);
	mask = _mm256_or_si256(mask, tilemsk);

	/* load the tiles in the grid */
	/* set the MSB of each entry to circumvent pcmpistri's string termination check */
	__m256i msbmsk = _mm256_set_epi64x(0x0000000000000080ULL, 0x8080808080808080ULL,
	    0x8080808080808080ULL, 0x8080808080808080ULL);
	__m256i grid = _mm256_or_si256(msbmsk, _mm256_loadu_si256((const __m256i*)p->grid));
	__m128i gridlo = _mm256_castsi256_si128(grid), gridhi = _mm256_extracti128_si256(grid, 1);
	__m128i masklo = _mm256_castsi256_si128(mask), maskhi = _mm256_extracti128_si256(mask, 1);

	/* compute the bitmasks with some shuffling */
#define OPERATION (_SIDD_UBYTE_OPS|_SIDD_CMP_EQUAL_ANY|_SIDD_BIT_MASK)
	__m128i maplolo = _mm_cmpistrm(masklo, gridlo, OPERATION);
	__m128i maphilo = _mm_cmpistrm(masklo, gridhi, OPERATION);
	maplolo = _mm_unpacklo_epi16(maplolo, maphilo);
	__m128i maplohi = _mm_cmpistrm(maskhi, gridlo, OPERATION);
	__m128i maphihi = _mm_cmpistrm(maskhi, gridhi, OPERATION);
	maplohi = _mm_unpacklo_epi16(maplohi, maphihi);
	maplolo = _mm_or_si128(maplolo, maplohi);
#undef OPERATION

	return (_mm_cvtsi128_si32(maplolo));
#elif defined(__SSE4_2__)
	/*
	 * this code is very similar to the AVX code except for the more
	 * complex masking in the beginning due to the lack of 256 bit
	 * registers.
	 */

	/*
	 * first, create a mask of those spots we are not interested in.
	 * We mark spots beyond the end of the tiles array as "interesting"
	 * under the assumption that they are zero.  This allows us to use
	 * pcmpistrm instead of the more expensive pcmpestrm.
	 */
	__m128i shufmasklo = _mm_set_epi64x(0x0101010101010101ULL,0x0000000000000000ULL);
	__m128i shufmaskhi = _mm_set_epi64x(0x0303030303030303ULL,0x0202020202020202ULL);
	__m128i bitmask = _mm_set1_epi64x(0x8040201008040201ULL);
	__m128i masklo = _mm_cvtsi32_si128(tileset_complement(ts)), maskhi;
	maskhi = _mm_shuffle_epi8(masklo, shufmaskhi);
	masklo = _mm_shuffle_epi8(masklo, shufmasklo);
	masklo = _mm_cmpeq_epi8(_mm_and_si128(masklo, bitmask), bitmask);
	maskhi = _mm_cmpeq_epi8(_mm_and_si128(maskhi, bitmask), bitmask);

	/* create sets of grid positions from the masks */
	__m128i tilemsklo = _mm_set_epi64x(0x8f8e8d8c8b8a8988ULL, 0x8786858483828180ULL);
	__m128i tilemskhi = _mm_set_epi64x(0x0000000000000098ULL, 0x9796959493929190ULL);
	masklo = _mm_or_si128(masklo, tilemsklo);
	maskhi = _mm_or_si128(maskhi, tilemskhi);

	/* load the tiles in the grid */
	/* set the MSB of each entry to circumvent pcmpistri's string termination check */
	__m128i lomsbmsk = _mm_set_epi64x(0x8080808080808080ULL, 0x8080808080808080ULL);
	__m128i himsbmsk = _mm_set_epi64x(0x0000000000000080ULL, 0x8080808080808080ULL);

	__m128i gridlo = _mm_or_si128(lomsbmsk, _mm_loadu_si128((const __m128i*)p->grid + 0));
	__m128i gridhi = _mm_or_si128(himsbmsk, _mm_loadu_si128((const __m128i*)p->grid + 1));

	/* compute the bitmasks */
#define OPERATION (_SIDD_UBYTE_OPS|_SIDD_CMP_EQUAL_ANY|_SIDD_BIT_MASK)
	__m128i maplolo = _mm_cmpistrm(masklo, gridlo, OPERATION);
	__m128i maphilo = _mm_cmpistrm(masklo, gridhi, OPERATION);
	maplolo = _mm_unpacklo_epi16(maplolo, maphilo);
	__m128i maplohi = _mm_cmpistrm(maskhi, gridlo, OPERATION);
	__m128i maphihi = _mm_cmpistrm(maskhi, gridhi, OPERATION);
	maplohi = _mm_unpacklo_epi16(maplohi, maphihi);
	maplolo = _mm_or_si128(maplolo, maplohi);
#undef OPERATION

	return (_mm_cvtsi128_si32(maplolo));
#else
	tileset map = EMPTY_TILESET;

	for (; !tileset_empty(ts); ts = tileset_remove_least(ts))
		map |= 1 << p->tiles[tileset_get_least(ts)];

	return (map);
#endif
}

/*
 * Given a tileset eq representing some squares of the grid, return a
 * tileset composed of those squares in eq which are adjacent to a
 * square not in eq.  Intuitively, these are the squares from which a
 * move could possibly lead to a configuration in a different
 * equivalence class.
 */
static inline tileset
tileset_reduce_eqclass(tileset eq)
{
	tileset c = tileset_complement(eq);

	/*
	 * the mask prevents carry into other rows:
	 * 0x0f7bdef: 01111 01111 01111 01111 01111
	 */
	return (eq & (c | c  << 5 | (c & 0x0f7bdef) << 1 | c >> 5 | c >> 1 & 0x0f7bdef));
}

/*
 * Given a puzzle configuration p, a tileset ts and a tileset eq =
 * tileset_eqclass(ts, p), return if p is the canonical configuration in
 * p.  A configuration in the equivalence class is canonical, if it has
 * the lowest zero position out of all configurations in the equivalence
 * class that are equal with respect to ts.  Return nonzero if this is
 * the canonical configuration, zero if it is not.
 */
static inline int
tileset_is_canonical(tileset ts, tileset eq, const struct puzzle *p)
{

	if (tileset_has(ts, ZERO_TILE))
		return (zero_location(p) == tileset_get_least(eq));
	else
		return (1);
}

/*
 * The rank of a tileset.  Given a tileset ts, its rank is the position
 * in lexicographic order it has among all tilesets with the same tile
 * count.
 */
typedef unsigned tsrank;

enum { RANK_SPLIT1 = 11, RANK_SPLIT2 = 18 };

/* ranktbl.c generated by util/rankgen */
extern const unsigned short rank_tails[1 << RANK_SPLIT1];
extern const tsrank rank_mids[RANK_SPLIT1 + 1][1 << RANK_SPLIT2 - RANK_SPLIT1];
extern const tsrank rank_heads[RANK_SPLIT2 + 1][1 << TILE_COUNT - RANK_SPLIT2];

/* rank.c */
extern const tileset *unrank_tables[TILE_COUNT + 1];
extern const tsrank combination_count[TILE_COUNT + 1];

extern void	tileset_unrank_init(size_t);

/*
 * Compute the rank of a tile set.  tileset_rank_init() must have been
 * called before this function.
 */
static inline tsrank
tileset_rank(tileset ts)
{
	tileset tail = tileset_intersect(ts, tileset_least(RANK_SPLIT1));
	tileset mid  = tileset_intersect(ts, tileset_least(RANK_SPLIT2));
	tileset head = ts >> RANK_SPLIT2;

	return (rank_tails[tail] + rank_mids[tileset_count(tail)][mid >> RANK_SPLIT1]
	    + rank_heads[tileset_count(mid)][head]);
}

/*
 * Compute the tileset with k tiles belonging to rank rk.  Initialize
 * the appropriate unrank table if necessary.  This is done as-needed
 * instead of ahead of time to simplify the code as unranking is not
 * nearly as time sensitive as ranking.
 */
static inline tileset
tileset_unrank(size_t k, tsrank rk)
{

	if (unrank_tables[k] == NULL)
		tileset_unrank_init(k);

	return (unrank_tables[k][rk]);
}

#endif /* TILESET_H */
