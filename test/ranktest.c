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

/* ranktest.c -- verify tileset_rank() and tileset_unrank() */

#include <stdlib.h>
#include <stdio.h>

#include "tileset.h"

/*
 * Compute the binomial coefficient (n choose k).
 */
static unsigned choose(unsigned n, unsigned k)
{
	if (k == 0)
		return (1);

	if (k > n)
		return (0);

	if (k > n/2)
		return (choose(n, n - k));

	return (n * (unsigned long long)choose(n - 1, k - 1) / k);
}

/*
 * Compute the rank of m using a direct algorithm.
 */
static tsrank naive_rank(tileset m)
{
	unsigned sum = 0, i = 1;

	for (; !tileset_empty(m); m = tileset_remove_least(m))
		sum += choose(tileset_get_least(m), i++);

	return (sum);
}

/*
 * Compute the combination belonging to a certain rank using a direct
 * algorithm.
 */
static tileset naive_unrank(size_t k, tsrank rk)
{
	size_t i = 24;
	tileset ts = EMPTY_TILESET;

	while (k > 0) {
		while (choose(i, k) > rk)
			i--;

		ts = tileset_add(ts, i);
		rk -= choose(i--, k--);
	}

	return (ts);
}

/*
 * Check if tileset_rank() works correctly.  Return 0 and print a
 * diagnostic message if there is an input where it does not.  Return 1
 * if it works correctly.
 */
static int
test_rank(void)
{
	tileset i;
	tsrank rk, nrk;

	for (i = EMPTY_TILESET; i <= FULL_TILESET; i++) {
		rk = tileset_rank(i);
		nrk = naive_rank(i);

		if (rk != nrk) {
			printf("rank mismatch: %07xu ranks to %u != %u\n", i, rk, nrk);
			return (0);
		}
	}

	return (1);
}

/*
 * Check if tileset_unrank() works correctly. Return 0 and print a
 * diagnostic message if there is an input where it does not.  Return 1
 * if it works correctly.
 */
static int
test_unrank(void)
{
	size_t k;
	tsrank i;
	tileset ts, nts;

	for (k = 0; k <= TILE_COUNT; k++) {
		tileset_unrank_init(k);

		for (i = 0; i < combination_count[k]; i++) {
			ts = tileset_unrank(k, i);
			nts = naive_unrank(k, i);

			if (ts != nts) {
				printf("unrank mismatch: %zu/%u unranks to %07xu != %07xu\n", k, i, ts, nts);
				return (0);
			}
		}
	}

	return (1);
}

extern int
main()
{
	if (!test_rank())
		return (EXIT_FAILURE);

	if (!test_unrank())
		return (EXIT_FAILURE);

	return (EXIT_SUCCESS);
}
