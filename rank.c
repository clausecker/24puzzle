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

/* rank.c -- tileset ranking and unranking */

#include <stdlib.h>
#include <stdio.h>

#include "puzzle.h"
#include "tileset.h"

/*
 * These tables store lookup tables for ranking positions.  Since we
 * typically only want to rank for one specific tile count, it is a
 * sensible choice to initialize the tables only as needed using
 * dynamic memory allocation.
 */
const tileset *unrank_tables[TILE_COUNT + 1] = {};

/*
 * Number of combinations for k items out of TILE_COUNT.  This is just
 * (TILE_COUNT choose k).  This lookup table is used to compute the
 * table sizes in tileset_unrank_init() and for various other purposes.
 */
const tsrank
combination_count[TILE_COUNT + 1] = {
	1,
	25,
	300,
	2300,
	12650,
	53130,
	177100,
	480700,
	1081575,
	2042975,
	3268760,
	4457400,
	5200300,
	5200300,
	4457400,
	3268760,
	2042975,
	1081575,
	480700,
	177100,
	53130,
	12650,
	2300,
	300,
	25,
	1,
};

/*
 * Allocate and initialize the unrank table for k bits out of
 * TILE_COUNT.  If memory allocation fails, abort the program.
 */
extern void
tileset_unrank_init(size_t k)
{
	size_t i, n = combination_count[k];
	tileset iter, *tbl;

	tbl = malloc(n * sizeof *unrank_tables[k]);
	if (tbl == NULL) {
		perror("malloc");
		abort();
	}

	for (i = 0, iter = (1 << k) - 1; i < n; i++, iter = next_combination(iter))
		tbl[i] = iter;

	unrank_tables[k] = tbl;
}
