/* puzzle.c -- manipulating struct puzzle */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __SSSE3__
# include <immintrin.h>
#endif

#include "puzzle.h"

/*
 * A solved puzzle configuration.
 */
const struct puzzle solved_puzzle = {
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
};


/*
 * List of all possible moves for a given position of the empty square.
 * There are up to four moves from every square, if there are less, the
 * remainder is filled up with -1.
 */
const signed char movetab[TILE_COUNT][4] = {
	 1,  5, -1, -1,
	 0,  2,  6, -1,
	 1,  3,  7, -1,
	 2,  4,  8, -1,
	 3,  9, -1, -1,

	 0,  6, 10, -1,
	 1,  5,  7, 11,
	 2,  6,  8, 12,
	 3,  7,  9, 13,
	 4,  8, 14, -1,

	 5, 11, 15, -1,
	 6, 10, 12, 16,
	 7, 11, 13, 17,
	 8, 12, 14, 18,
	 9, 13, 19, -1,

	10, 16, 20, -1,
	11, 15, 17, 21,
	12, 16, 18, 22,
	13, 17, 19, 23,
	14, 18, 24, -1,

	15, 21, -1, -1,
	16, 20, 22, -1,
	17, 21, 23, -1,
	18, 22, 24, -1,
	19, 23, -1, -1,
};

/*
 * The grid locations transposed along the primary diagonal.
 */
const unsigned char transpositions[TILE_COUNT] = {
	 0,  5, 10, 15, 20,
	 1,  6, 11, 16, 21,
	 2,  7, 12, 17, 22,
	 3,  8, 13, 18, 23,
	 4,  9, 14, 19, 24,
};

#ifdef __AVX2__
/*
 * Compose permutations p and q using pshufb.  As pshufb permutes
 * within lanes, we need to split the composition into two shuffles and
 * then a recombination step.
 */
static __m256i
compose_avx(__m256i p, __m256i q)
{
	__m256i fifteen = _mm256_set1_epi8(15), sixteen = _mm256_set1_epi8(16);
	__m256i plo, phi, qlo, qhi;

	plo = _mm256_permute2x128_si256(p, p, 0x00);
	phi = _mm256_permute2x128_si256(p, p, 0x11);

	qlo = _mm256_or_si256(q, _mm256_cmpgt_epi8(q, fifteen));
	qhi = _mm256_sub_epi8(q, sixteen);

	return (_mm256_or_si256(_mm256_shuffle_epi8(plo, qlo), _mm256_shuffle_epi(phi, qhi));
}
#endif

#ifdef __SSSE3__
/*
 * Compose permutations p and q using pshufb.  This is similar to
 * compose_avx, but we store the parts to rlo and rhi as we cannot
 * return more than one value in C.
 */
static void
compose_sse(__m128i *rlo, __m128i *rhi, __m128i plo, __m128i phi, __m128i qlo, __m128i qhi)
{
	__m128i fifteen = _mm_set1_epi8(15), sixteen = _mm_set1_epi8(16);
	__m128i qlolo, qlohi, qhilo, qhihi;

	qlolo = _mm_or_si128(qlo, _mm_cmpgt_epi8(qlo, fifteen));
	qhilo = _mm_or_si128(qhi, _mm_cmpgt_epi8(qhi, fifteen));

	qlohi = _mm_sub_epi8(qlo, sixteen);
	qhihi = _mm_sub_epi8(qhi, sixteen);

	*rlo = _mm_or_si128(_mm_shuffle_epi8(plo, qlolo), _mm_shuffle_epi8(phi, qlohi));
	*rhi = _mm_or_si128(_mm_shuffle_epi8(plo, qhilo), _mm_shuffle_epi8(phi, qhihi));
}
#endif

/*
 * Transpose p along the main diagonal.  If * is the composition of
 * permutations, the following operation is performed:
 *
 *     grid = transpositions * grid * transpositions
 *     tiles = transpositions * tiles * transpositions
 *
 * Note that this simple formula obtains as transpositions is an
 * involution.  p and its transposition have the same distance to the
 * solved puzzle by construction, so we can lookup both a puzzle and its
 * transposition in the PDB and take the maximum of the two values to
 * get a better heuristic.
 */
extern void
transpose(struct puzzle *p)
{
#ifdef __AVX2__
	/* transposition mask */
	__m256i tmask = _mm256_setr_epi8(
	     0,  5, 10, 15, 20,
	     1,  6, 11, 16, 21,
	     2,  7, 12, 17, 22,
	     3,  8, 13, 18, 23,
	     4,  9, 14, 19, 24,
	    -1, -1, -1, -1, -1, -1, -1);

	__m256i tiles = _mm256_loadu_si256((__m256i*)p->tiles);
	tiles = compose_avx(tmask, compose_avx(tiles, tmask));
	_mm256_storeu_si256((__m256i*)p->tiles, tiles);

	__m256i grid = _mm256_loadu_si256((__m256i*)p->grid);
	grid = compose_avx(tmask, compose_avx(grid, tmask));
	_mm256_storeu_si256((__m256i*)p->grid, grid);
#elif defined(__SSSE3__)
	/* transposition mask */
	__m128i tmasklo = _mm_setr_epi8( 0,  5, 10, 15, 20,  1,  6, 11, 16, 21,  2,  7, 12, 17, 22,  3);
	__m128i tmaskhi = _mm_setr_epi8( 8, 13, 18, 23,  4,  9, 14, 19, 24, -1, -1, -1, -1, -1, -1, -1);

	__m128i tileslo = _mm_loadu_si128((__m128i*)p->tiles + 0);
	__m128i tileshi = _mm_loadu_si128((__m128i*)p->tiles + 1);
	compose_sse(&tileslo, &tileshi, tileslo, tileshi, tmasklo, tmaskhi);
	compose_sse(&tileslo, &tileshi, tmasklo, tmaskhi, tileslo, tileshi);
	_mm_storeu_si128((__m128i*)p->tiles + 0, tileslo);
	_mm_storeu_si128((__m128i*)p->tiles + 1, tileshi);

	__m128i gridlo = _mm_loadu_si128((__m128i*)p->grid + 0);
	__m128i gridhi = _mm_loadu_si128((__m128i*)p->grid + 1);
	compose_sse(&gridlo, &gridhi, gridlo, gridhi, tmasklo, tmaskhi);
	compose_sse(&gridlo, &gridhi, tmasklo, tmaskhi, gridlo, gridhi);
	_mm_storeu_si128((__m128i*)p->grid + 0, gridlo);
	_mm_storeu_si128((__m128i*)p->grid + 1, gridhi);
#else
	size_t i;

	for (i = 0; i < TILE_COUNT; i++) {
		p->tiles[i] = transpositions[p->tiles[transpositions[i]]];
		p->grid[p->tiles[i]] = i;
	}
#endif
}

/*
 * Describe p as a string and write the result to str.
 */
extern void
puzzle_string(char str[PUZZLE_STR_LEN], const struct puzzle *p)
{
	size_t i;

	for (i = 0; i < TILE_COUNT; i++)
		sprintf(str + 3 * i, "%2d ", p->tiles[i]);

	for (i = 0; i < TILE_COUNT; i++)
		sprintf(str + 3 * TILE_COUNT + 3 * i, "%2d ", p->grid[i]);

	str[3 * TILE_COUNT - 1] = '\n';
	str[2 * 3 * TILE_COUNT - 1] = '\n';
}

/*
 * Parse a puzzle configuration from str and store it in p.  Return 0
 * if parsing was succesful, -1 otherwise.  In case of failure, *p is
 * undefined.
 */
extern int
puzzle_parse(struct puzzle *p, const char *str)
{
	size_t i;
	long component;

	memset(p, 0, sizeof *p);
	memset(p->tiles, 0xff, TILE_COUNT);

	for (i = 0; i < TILE_COUNT; i++) {
		component = strtol(str, (char **)&str, 10);
		if (component < 0 || component >= TILE_COUNT)
			return (-1);

		if (p->tiles[component] != 0xff)
			return (-1);

		while (isspace(*str))
			str++;

		if (*str != ',' && i < TILE_COUNT - 1)
			return (-1);

		p->grid[i] = component;
		p->tiles[component] = i;

		str++;
	}

	return (0);
}
