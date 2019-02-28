/*-
 * Copyright (c) 2019 Robert Clausecker. All rights reserved.
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

/* index_avx512.c -- vectorised index function for AVX512 */

#include "index.h"
#include "puzzle.h"
#include "tileset.h"

#if __AVX512BW__ && __AVX512VL__ && __AVX512CD__
#include <immintrin.h>

/*
 * Compute the population count of 16 integers of 32 bit each.  Use
 * the relevant intrinsic if possible, otherwise fall back to bit
 * fiddling tricks.
 */
static __m512i popcount16_32(__m512i x)
{
#ifdef __AVX512VPOPCNTDQ__
	return (_mm512_popcnt_epi32(x));
#else
	__m512i mask;

	/* x = x - (x >> 1 & 0x55555555) */
	x = _mm512_sub_epi32(x, _mm512_and_epi32(_mm512_srli_epi32(x, 1), _mm512_set1_epi32(0x55555555)));

	/* x = (x >> 2 & 0x33333333) + (x & 0x33333333) */
	mask = _mm512_set1_epi32(0x33333333);
	x = _mm512_add_epi32(_mm512_and_epi32(_mm512_srli_epi32(x, 2), mask), _mm512_and_epi32(x, mask));

	/* x = (x >> 4) + (x & 0x0f0f0f0f) */
	x = _mm512_add_epi32(_mm512_srli_epi32(x, 4), _mm512_and_epi32(x, _mm512_set1_epi32(0x0f0f0f0f)));

	/* x = (x >> 8) + (x & 0x00ff00ff) */
	x = _mm512_add_epi32(_mm512_srli_epi32(x, 8), _mm512_and_epi32(x, _mm512_set1_epi32(0x00ff00ff)));

	/* x = (x >> 16) + (x & 0x0000ffff) */
	x = _mm512_add_epi32(_mm512_srli_epi32(x, 16), _mm512_and_epi32(x, _mm512_set1_epi32(0x0000ffff)));

	return (x);
#endif
}

/*
 * Compute 16 indices into 6 tile APDBs at once.  Combine the indices into
 * offsets and store them to idx.
 *
 * TODO: add some way to mask the indices.
 */
extern void
compute_index_16a6(unsigned pidxbuf[restrict 16], unsigned maprank[restrict 16],
    const struct puzzle *p, const tileset tsarg[restrict 16])
{
	__m512i tiles, grid, ts; /* arguments */
	__m512i zero, one, thirtyone; /* constants */
	__m512i t, least, x, y, tail, mid, head, count, factor; /* immediate values */
	__m512i map, rank, pidx; /* result values */

	size_t i;

	/*
	 * AVX512BW lacks useful byte permute instructions.  To cope with
	 * this, we expand p->tiles and p->grid to 16 bit elements.
	 */
	tiles = _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i *)p->tiles));
	grid = _mm512_cvtepu8_epi16(_mm256_loadu_si256((const __m256i *)p->tiles));
	ts = _mm512_loadu_si512(tsarg);

	/* common constants */
	zero = _mm512_setzero_epi32();
	one = _mm512_set1_epi32(1);
	thirtyone = _mm512_set1_epi32(31);

	/*
	 * Compute tile_map() for all tile sets.  This is done as in
	 * the unvectorised version as iterating the loop 6 times is
	 * faster than issuing pcmpistrm 16 times (or so I think).
	 */
	t = ts;
	map = zero;

	for (i = 0; i < 6; i++) {
		/* least = t & -t (get least bit) */
		least = _mm512_and_epi32(t, _mm512_sub_epi32(zero, t));

		/* x = 31 - clz(least) (which gives us ctz(t)) */
		x = _mm512_sub_epi32(thirtyone, _mm512_lzcnt_epi32(least));

		/* x = tiles[ctz(t)] (find tile location) */
		x = _mm512_cvtepu16_epi32(_mm512_castsi512_si256(_mm512_permutex2var_epi16(tiles,
		    _mm512_castsi256_si512(_mm512_cvtepi32_epi16(x)), zero)));

		/* map |= 1 << x (add tile to map) */
		map = _mm512_or_epi32(_mm512_sllv_epi16(one, x), map);

		/* t &= ~least (clear least bit) */
		t = _mm512_andnot_epi32(least, t);
	}

	/*
	 * Compute tileset_rank() for each map.
	 * This uses the auxillary popcount function.
	 */
	tail = _mm512_and_epi32(map, _mm512_set1_epi32(tileset_least(RANK_SPLIT1)));
	mid = _mm512_and_epi32(map, _mm512_set1_epi32(tileset_least(RANK_SPLIT2)));
	head = _mm512_srli_epi32(map, RANK_SPLIT2);

	/* rank = rank_tails[tail] */
	rank = _mm512_i32gather_epi32(tail, rank_tails, 4);

	/* rank += rank_mids[tileset_count(tail)][mid >> RANK_SPLIT1] */
	count = popcount16_32(tail);
	x = _mm512_add_epi32(_mm512_slli_epi32(count, RANK_SPLIT2 - RANK_SPLIT1), _mm512_srli_epi32(mid, RANK_SPLIT1));
	rank = _mm512_add_epi32(rank, _mm512_i32gather_epi32(x, rank_mids, 4));

	/* rank += rank_heads[tileset_count(mid)][head]) */
	/* where tileset_count(mid) = tileset_count(midshift) + tileset_count(tail) */
	count = popcount16_32(mid);
	x = _mm512_add_epi32(_mm512_slli_epi32(count, TILE_COUNT - RANK_SPLIT2), head);
	rank = _mm512_add_epi32(rank, _mm512_i32gather_epi32(x, rank_heads, 4));

	/*
	 * Compute index_permutation() for each map.
	 * This too uses the auxillary popcount function.
	 */
	pidx = zero;
	factor = one;
	t = ts;
	for (i = 0; i < 6; i++) {
		/* least = t & -t (get least bit) */
		least = _mm512_and_epi32(t, _mm512_sub_epi32(zero, t));

		/* x = 31 - clz(least) (which gives us ctz(t)) */
		x = _mm512_sub_epi32(thirtyone, _mm512_lzcnt_epi32(least));

		/* x = tiles[ctz(t)] (find grid location) */
		x = _mm512_cvtepu16_epi32(_mm512_castsi512_si256(_mm512_permutex2var_epi16(tiles,
		    _mm512_castsi256_si512(_mm512_cvtepi32_epi16(x)), zero)));

		/* y = 1 << x (tile at index x) */
		y = _mm512_sllv_epi32(one, x);

		/* x = cnt(y - 1 & map) (inversion number) */
		x = popcount16_32(_mm512_and_epi32(_mm512_sub_epi32(y, one), map));

		/* pidx += factor * x */
		pidx = _mm512_add_epi32(pidx, _mm512_mullo_epi32(factor, x));

		/* factor *= 6 - i */
		factor = _mm512_mullo_epi32(factor, _mm512_set1_epi32(6 - i));

		/* t &= ~least (clear least bit) */
		t = _mm512_andnot_epi32(least, t);

		/* map &= ~y (clear bit in inversion map) */
		map = _mm512_andnot_epi32(y, map);
	}

	_mm512_storeu_si512(pidxbuf, pidx);
	_mm512_storeu_si512(maprank, rank);
}

#else /* __AVX512BW__ && __AVX512VL__ && __AVX512CD__ */
# error TODO: implement fallback
#endif
