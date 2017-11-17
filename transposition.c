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

/* transposition.c -- transpose puzzles */

#include <assert.h>
#include <stdalign.h>
#include <string.h>

#ifdef __SSSE3__
# include <immintrin.h>
#endif

#include "tileset.h"
#include "puzzle.h"
#include "transposition.h"

/*
 * All the ways the puzzle tray can be rotated and transposed.  For
 * each possible rotation / transposition, the permutation vector and
 * its inverse are stored.  The rightmost dimension is 32 instead of
 * TILE_COUNT for alignment.  The whole array is aligned to 64 bytes
 * so vector instructions can be used to access it without misalignment
 * penalties.  This array is called automorphisms because transposing
 * and rotating the tray are its automorphisms with respect to the
 * sliding to the sliding-tile puzzle's possible moves.
 */
#define PAD -1, -1, -1, -1, -1, -1, -1
alignas(64) const unsigned char automorphisms[AUTOMORPHISM_COUNT][2][32] = {
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, PAD,
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, PAD,

	20, 15, 10,  5,  0, 21, 16, 11,  6,  1, 22, 17, 12,  7,  2, 23, 18, 13,  8,  3, 24, 19, 14,  9,  4, PAD,
	 4,  9, 14, 19, 24,  3,  8, 13, 18, 23,  2,  7, 12, 17, 22,  1,  6, 11, 16, 21,  0,  5, 10, 15, 20, PAD,

	24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0, PAD,
	24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0, PAD,

	 4,  9, 14, 19, 24,  3,  8, 13, 18, 23,  2,  7, 12, 17, 22,  1,  6, 11, 16, 21,  0,  5, 10, 15, 20, PAD,
	20, 15, 10,  5,  0, 21, 16, 11,  6,  1, 22, 17, 12,  7,  2, 23, 18, 13,  8,  3, 24, 19, 14,  9,  4, PAD,


	 0,  5, 10, 15, 20,  1,  6, 11, 16, 21,  2,  7, 12, 17, 22,  3,  8, 13, 18, 23,  4,  9, 14, 19, 24, PAD,
	 0,  5, 10, 15, 20,  1,  6, 11, 16, 21,  2,  7, 12, 17, 22,  3,  8, 13, 18, 23,  4,  9, 14, 19, 24, PAD,

	20, 21, 22, 23, 24, 15, 16, 17, 18, 19, 10, 11, 12, 13, 14,  5,  6,  7,  8,  9,  0,  1,  2,  3,  4, PAD,
	 4,  3,  2,  1,  0,  9,  8,  7,  6,  5, 14, 13, 12, 11, 10, 19, 18, 17, 16, 15, 24, 23, 22, 21, 20, PAD,

	24, 19, 14,  9,  4, 23, 18, 13,  8,  3, 22, 17, 12,  7,  2, 21, 16, 11,  6,  1, 20, 15, 10,  5,  0, PAD,
	24, 19, 14,  9,  4, 23, 18, 13,  8,  3, 22, 17, 12,  7,  2, 21, 16, 11,  6,  1, 20, 15, 10,  5,  0, PAD,

	 4,  3,  2,  1,  0,  9,  8,  7,  6,  5, 14, 13, 12, 11, 10, 19, 18, 17, 16, 15, 24, 23, 22, 21, 20, PAD,
	20, 21, 22, 23, 24, 15, 16, 17, 18, 19, 10, 11, 12, 13, 14,  5,  6,  7,  8,  9,  0,  1,  2,  3,  4, PAD,
};
#undef PAD

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

	return (_mm256_or_si256(_mm256_shuffle_epi8(plo, qlo), _mm256_shuffle_epi8(phi, qhi)));
}
#elif defined(__SSSE3__)
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
	__m256i tmask = _mm256_load_si256((__m256i*)transpositions);

	__m256i tiles = _mm256_loadu_si256((__m256i*)p->tiles);
	tiles = compose_avx(tmask, compose_avx(tiles, tmask));
	_mm256_storeu_si256((__m256i*)p->tiles, tiles);

	__m256i grid = _mm256_loadu_si256((__m256i*)p->grid);
	grid = compose_avx(tmask, compose_avx(grid, tmask));
	_mm256_storeu_si256((__m256i*)p->grid, grid);
#elif defined(__SSSE3__)
	/* transposition mask */
	__m128i tmasklo = _mm_load_si128((__m128i*)transpositions + 0);
	__m128i tmaskhi = _mm_load_si128((__m128i*)transpositions + 1);

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
	unsigned char otiles[TILE_COUNT];

	memcpy(otiles, p->tiles, sizeof otiles);

	for (i = 0; i < TILE_COUNT; i++) {
		p->tiles[i] = transpositions[otiles[transpositions[i]]];
		p->grid[p->tiles[i]] = i;
	}
#endif
}

/*
 * Morph puzzle p using automorphism a.  The PDB entry for p under some
 * tile set ts is equal to the PDB entry for morph(p, a) under tile set
 * tileset_morph(ts, a).
 */
extern void
morph(struct puzzle *p, unsigned a)
{
	size_t i;
	unsigned char otiles[TILE_COUNT];

	assert(a < AUTOMORPHISM_COUNT);

	memcpy(otiles, p->tiles, sizeof otiles);

	for (i = 0; i < TILE_COUNT; i++) {
		p->tiles[i] = automorphisms[a][0][otiles[automorphisms[a][1][i]]];
		p->grid[p->tiles[i]] = i;
	}
}

/*
 * Send tile set ts through automorphism a and return the resulting tile
 * set.
 */
extern tileset
tileset_morph(tileset ts, unsigned a)
{
	tileset t = EMPTY_TILESET;

	assert(a < AUTOMORPHISM_COUNT);

	for (; !tileset_empty(ts); ts = tileset_remove_least(ts))
		t = tileset_add(t, automorphisms[a][0][tileset_get_least(ts)]);

	return (t);
}


/*
 * Given a tile set ts, find the automorphism leading to the
 * lexicographically least tile set whose PDB computes the same
 * distances as this one.  This function does the right thing both
 * for zero-unaware and zero-aware pattern databases.
 */
extern unsigned
canonical_automorphism(tileset ts)
{
	unsigned i, min, has_zero_tile = tileset_has(ts, ZERO_TILE);
	tileset mints, r, morphts;

	/* i == 0 is the identity and needs not be checked */
	ts = tileset_remove(ts, ZERO_TILE);
	mints = ts;
	min = 0;

	/*
	 * r is the region the zero tile is in in the solved
	 * configuration.  For the PDB to compute the same
	 * distances, this region must be identical in the
	 * morphed solved configuration.
	 */
	r = tileset_complement(ts);
	if (has_zero_tile)
		r = tileset_flood(r, ZERO_TILE);

	for (i = 1; i < AUTOMORPHISM_COUNT; i++) {
		morphts = tileset_morph(ts, i);
		if (morphts >= mints)
			continue;

		/*
		 * Check if automorphism i preserves the region the
		 * solved configuration is in.
		 */
		if (tileset_has(tileset_morph(r, i), ZERO_TILE)) {
			mints = morphts;
			min = i;
		}
	}

	return (min);
}
