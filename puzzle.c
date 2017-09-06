/* puzzle.c -- manipulating struct puzzle */

#include <assert.h>
#include <stdio.h>

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
const signed char movetab[25][4] = {
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
 * Transpose p along the main diagonal.  p and its transposition have
 * the same distance to the solved puzzle by construction, so we can
 * lookup both a puzzle and its transposition in the PDB and take the
 * maximum of the two values to get a better heuristic.
 */
extern void
transpose(struct puzzle *p)
{
#ifdef __AVX2__
	/*
	 * Both vector implementations use pshufb (_mm_shuffle_epi8) to
	 * implement the composition of permutations.  tmask is the
	 * permutation that performs our transposition.  It is its own
	 * inverse, so the transposed tiles array is just tiles * tmask
	 * and the transposed grid array is just tmask * grid since grid
	 * is the inverse of tiles.  Because pshufb can only shuffle 16
	 * array entries at a time, we have to split up the composition
	 * and do four shuffles for each.
	 */

	__m256i fifteen = _mm256_set1_epi8(15), sixteen = _mm256_set1_epi8(16);

	/* transposition mask */
	__m256i tmask = _mm256_setr_epi8(
	     0,  5, 10, 15, 20,
	     1,  6, 11, 16, 21,
	     2,  7, 12, 17, 22,
	     3,  8, 13, 18, 23,
	     4,  9, 14, 19, 24,
	    -1, -1, -1, -1, -1, -1, -1);

	__m256i tiles = _mm256_loadu_si256((__m256i*)p->tiles), ttiles;
	__m256i grid = _mm256_loadu_si256((__m256i*)p->grid), tgrid;

	/* prepare pshufb masks */
	__m256i tileslo = _mm256_or_si256(tiles, _mm256_cmpgt_epi8(tiles, fifteen));
	__m256i tileshi = _mm256_sub_epi8(tiles, sixteen);

	__m256i tmasklo = _mm256_or_si256(tmask, _mm256_cmpgt_epi8(tmask, fifteen));
	__m256i tmaskhi = _mm256_sub_epi8(tmask, sixteen);

	/* shuffle masks around to get them in the right order */
	__m256i tmasklolo = _mm256_permute2x128_si256(tmask, tmask, 0x00);
	__m256i tmaskhihi = _mm256_permute2x128_si256(tmask, tmask, 0x11);

	__m256i gridlolo = _mm256_permute2x128_si256(grid, grid, 0x00);
	__m256i gridhihi = _mm256_permute2x128_si256(grid, grid, 0x11);

	ttiles = _mm256_or_si256(_mm256_shuffle_epi8(tmasklolo, tileslo), _mm256_shuffle_epi8(tmaskhihi, tileshi));
	tgrid = _mm256_or_si256(_mm256_shuffle_epi8(gridlolo, tmasklo), _mm256_shuffle_epi8(gridhihi, tmaskhi));

	_mm256_storeu_si256((__m256i*)p->tiles, ttiles);
	_mm256_storeu_si256((__m256i*)p->grid, tgrid);
#elif defined(__SSSE3__)
	__m128i fifteen = _mm_set1_epi8(15), sixteen = _mm_set1_epi8(16);

	/* transposition mask */
	__m128i tmasklo = _mm_setr_epi8( 0,  5, 10, 15, 20,  1,  6, 11, 16, 21,  2,  7, 12, 17, 22,  3);
	__m128i tmaskhi = _mm_setr_epi8( 8, 13, 18, 23,  4,  9, 14, 19, 24, -1, -1, -1, -1, -1, -1, -1);

	__m128i tileslo = _mm_loadu_si128((__m128i*)p->tiles + 0), ttileslo;
	__m128i tileshi = _mm_loadu_si128((__m128i*)p->tiles + 1), ttileshi;

	__m128i gridlo = _mm_loadu_si128((__m128i*)p->grid + 0), tgridlo;
	__m128i gridhi = _mm_loadu_si128((__m128i*)p->grid + 1), tgridhi;

	/* tiles split into pieces for pshufb */
	__m128i tileslolo = _mm_or_si128(tileslo, _mm_cmpgt_epi8(tileslo, fifteen));
	__m128i tileslohi = _mm_sub_epi8(tileslo, sixteen);
	__m128i tileshilo = _mm_or_si128(tileshi, _mm_cmpgt_epi8(tileshi, fifteen));
	__m128i tileshihi = _mm_sub_epi8(tileshi, sixteen);

	/* transposition mask split into pieces for pshufb */
	__m128i tmasklolo = _mm_or_si128(tmasklo, _mm_cmpgt_epi8(tmasklo, fifteen));
	__m128i tmasklohi = _mm_sub_epi8(tmasklo, sixteen);
	__m128i tmaskhilo = _mm_or_si128(tmaskhi, _mm_cmpgt_epi8(tmaskhi, fifteen));
	__m128i tmaskhihi = _mm_sub_epi8(tmaskhi, sixteen);

	ttileslo = _mm_or_si128(_mm_shuffle_epi8(tmasklo, tileslolo), _mm_shuffle_epi8(tmaskhi, tileslohi));
	ttileshi = _mm_or_si128(_mm_shuffle_epi8(tmasklo, tileshilo), _mm_shuffle_epi8(tmaskhi, tileshihi));

	tgridlo = _mm_or_si128(_mm_shuffle_epi8(gridlo, tmasklolo), _mm_shuffle_epi8(gridhi, tmasklohi));
	tgridhi = _mm_or_si128(_mm_shuffle_epi8(gridlo, tmaskhilo), _mm_shuffle_epi8(gridhi, tmaskhihi));

	_mm_storeu_si128((__m128i*)p->tiles + 0, ttileslo);
	_mm_storeu_si128((__m128i*)p->tiles + 1, ttileshi);

	_mm_storeu_si128((__m128i*)p->grid + 0, tgridlo);
	_mm_storeu_si128((__m128i*)p->grid + 1, tgridhi);
#else
	size_t i;
	static const unsigned char tmap[TILE_COUNT] = {
		 0,  5, 10, 15, 20,
		 1,  6, 11, 16, 21,
		 2,  7, 12, 17, 22,
		 3,  8, 13, 18, 23,
		 4,  9, 14, 19, 24,
	};

	for (i = 0; i < TILE_COUNT; i++) {
		p->tiles[i] = tmap[p->tiles[i]];
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
