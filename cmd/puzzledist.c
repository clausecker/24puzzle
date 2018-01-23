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

/* puzzledist.c -- compute the number of puzzles with the given distance */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "puzzle.h"
#include "builtins.h"

/*
 * To save storage, we store puzzles using a compact representation with
 * five bits per entry, not storing the position of the zero tile.
 * Additionally, four bits are used to store all moves that lead back to
 * the previous generation.  This leads to 24 * 5 + 4 = 124 bits of
 * storage being required in total, split into two 64 bit quantities.
 * The lo member starts with the four source bits and continues with 12
 * tile location entries.  The high member contains the other 12 tile
 * locations.
 */
struct compact_puzzle {
	unsigned long long lo, hi;
};

/*
 * Translate a struct puzzle into a struct compact_puzzle.
 */
void
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
		cp->lo |= (unsigned long long)p->tiles[i] << 5 * i + 4;

	for (; i < TILE_COUNT; i++)
		cp->hi |= (unsigned long long)p->tiles[i] << 5 * (i - 12);
#endif /* HAS_PDEP */
}

/*
 * Translate a struct compact_puzzle back into a struct puzzle.
 * TODO: pdep implementation.
 */
void
unpack_puzzle(struct puzzle *restrict p, const struct compact_puzzle *restrict cp)
{
#if HAS_PDEP == 1
	size_t i;
	unsigned long long scratch, data;
	__m256i grid, gridmask;

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

	/* if we have pdep, we also have AVX2 to find p->tiles[0] */
	grid = _mm256_loadu_si256((const __m256i*)&p->grid);
	gridmask = _mm256_cmpeq_epi8(grid, _mm256_setzero_si256());
	p->tiles[0] = ctz(_mm256_movemask_epi8(gridmask));
#else /* HAS_PDEP != 1 */
	size_t i;
	unsigned long long accum;

	memset(p, 0, sizeof *p);

	accum = cp->lo >> 4;
	for (i = 1; i <= 12; i++) {
		p->grid[accum & 31] = i;
		accum >>= 5;
	}

	accum = cp->hi;
	for (; i < TILE_COUNT; i++) {
		p->grid[accum & 31] = i;
		accum >>= 5;
	}

	/* the zero tile's location has been set to zero by memset before */
	for (i = 0; i < TILE_COUNT; i++)
		p->tiles[p->grid[i]] = i;
#endif /* HAS_PDEP */
}
