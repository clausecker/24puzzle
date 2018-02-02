/*-
 * Copyright (c) 2018 Robert Clausecker. All rights reserved.
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

/* compact.c -- compact puzzle representation */

#ifdef __SSE__
# include <immintrin.h>
#endif

#include <assert.h>
#include <string.h>


#include "compact.h"
#include "puzzle.h"
#include "builtins.h"

/*
 * Translate a struct puzzle into a struct compact_puzzle.
 */
extern void
pack_puzzle(struct compact_puzzle *restrict cp, const struct puzzle *restrict p)
{
#if HAS_PDEP == 1
	/* pext is available iff pdep is available */
	unsigned long long scratch, data;

	data = *(const unsigned long long*)&p->tiles[1];
	scratch = _pext_u64(data, 0x1f1f1f1f1f1f1f1full) << 4;
	data = *(unsigned*)&p->tiles[9];
	scratch |= (unsigned long long)_pext_u32(data, 0x1f1f1f1fu) << 4 + 8 * 5;

	cp->lo = scratch;

	data = *(const unsigned long long*)&p->tiles[13];
	scratch = _pext_u64(data, 0x1f1f1f1f1f1f1f1full);
	data = *(unsigned*)&p->tiles[21];
	scratch |= (unsigned long long)_pext_u32(data, 0x1f1f1f1fu) << 8 * 5;

	cp->hi = scratch;
#else /* HAS_PDEP != 1 */
	size_t i;

	cp->lo = 0;
	cp->hi = 0;

	for (i = 1; i <= 12; i++)
		cp->lo |= (unsigned long long)p->tiles[i] << 5 * (i - 1) + 4;

	for (; i < TILE_COUNT; i++)
		cp->hi |= (unsigned long long)p->tiles[i] << 5 * (i - 13);
#endif /* HAS_PDEP */
}

/*
 * Pack p into cp, masking out the move that leads to dest.
 */
extern void
pack_puzzle_masked(struct compact_puzzle *restrict cp, const struct puzzle *restrict p,
    int dest)
{
	size_t i, n_moves;
	int zloc;
	const signed char *moves;

	pack_puzzle(cp, p);
	zloc = zero_location(p);
	n_moves = move_count(zloc);
	moves = get_moves(zloc);

	for (i = 0; i < n_moves; i++)
		if (moves[i] == dest)
			cp->lo |= 1 << i;
}

/*
 * Translate a struct compact_puzzle back into a struct puzzle.
 */
extern void
unpack_puzzle(struct puzzle *restrict p, const struct compact_puzzle *restrict cp)
{
#if HAS_PDEP == 1
	size_t i;
	unsigned long long data;

	memset(p, 0, sizeof *p);

	data = _pdep_u64(cp->lo >> 4, 0x1f1f1f1f1f1f1f1full);
	*(unsigned long long *)&p->tiles[1] = data;
	data = _pdep_u32(cp->lo >> 4 + 8 * 5, 0x1f1f1f1fu);
	*(unsigned *)&p->tiles[9] = data;

	data = _pdep_u64(cp->hi, 0x1f1f1f1f1f1f1f1full);
	*(unsigned long long *)&p->tiles[13] = data;
	data = _pdep_u32(cp->hi >> 8 * 5, 0x1f1f1f1fu);
	*(unsigned *)&p->tiles[21] = data;

	for (i = 1; i < TILE_COUNT; i++)
		p->grid[p->tiles[i]] = i;
#else /* HAS_PDEP != 1 */
	size_t i;
	unsigned long long accum;

	memset(p, 0, sizeof *p);

	accum = cp->lo >> 4;
	for (i = 1; i <= 12; i++) {
		p->tiles[i] = accum & 31;
		p->grid[accum & 31] = i;
		accum >>= 5;
	}

	accum = cp->hi;
	for (; i < TILE_COUNT; i++) {
		p->tiles[i] = accum & 31;
		p->grid[accum & 31] = i;
		accum >>= 5;
	}
#endif /* HAS_PDEP */

	/* find the location of the zero tile in grid and set p->tiles[0] */
#ifdef __AVX2__
	__m256i grid, gridmask;

	grid = _mm256_loadu_si256((const __m256i*)&p->grid);
	gridmask = _mm256_cmpeq_epi8(grid, _mm256_setzero_si256());
	p->tiles[0] = ctz(_mm256_movemask_epi8(gridmask));
#elif defined(__SSE2__)
	__m128i gridlo, gridhi, gridmasklo, gridmaskhi, zero;

	gridlo = _mm_loadu_si128((const __m128i*)&p->grid + 0);
	gridhi = _mm_loadu_si128((const __m128i*)&p->grid + 1);

	zero = _mm_setzero_si128();

	gridmasklo = _mm_cmpeq_epi8(gridlo, zero);
	gridmaskhi = _mm_cmpeq_epi8(gridhi, zero);

	p->tiles[0] = ctz(_mm_movemask_epi8(gridmaskhi) << 16 | _mm_movemask_epi8(gridmasklo));
#else
	p->tiles[0] = strlen((const char *)p->grid);
#endif
}

/*
 * Compare two struct compact_puzzle in a manner suitable for qsort.
 */
extern int
compare_cp(const void *a_arg, const void *b_arg)
{
	const struct compact_puzzle *a = a_arg, *b = b_arg;

	if (a->hi != b->hi)
		return ((a->hi > b->hi) - (a->hi < b->hi));
	else
		return ((a->lo > b->lo) - (a->lo < b->lo));
}
