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

/* match.c -- find optimal 6-6-6-6 tile partitionings */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "builtins.h"
#include "match.h"
#include "tileset.h"
#include "heuristic.h"
#include "puzzle.h"

/*
 * Finding the optimal partitioning from the h values in a match vector
 * is essentially finding a maximum weighted matching in a hypergraph.
 * This is NP hard but solvable in realistic time as our problem
 * instance is rather small.  The naïve algorithm which tried out all
 * possible partitionings, finding the best ones, requires us to check
 * 96.197.645.544 partitionings.  This is a bit too much, so a smarter
 * algorithm is employed instead.  This algorithm first considers all
 * partitionings of the tray into two halves of 12 tiles each and then
 * finds the best partitioning for each half.  This requires only
 * (12 ! 24) * (1 + 2 * (6 ! 12) / 2!) / 2! = 1.250.672.150 operations,
 * a much less scary number.
 */
enum {
	TWELVE_TILES = 2704156, /* 24 choose 12 */
	SIX_OF_TWELVE = 924, /* 12 choose 6 */
};

static int	match_half_best(const unsigned char[MATCH_SIZE], tileset, tileset[2], unsigned long long *);

/*
 * Find the best way to partition the tray into 4 groups of six tiles
 * and store the optimal matches in match.  On success return 1,  on
 * error, return 0 and set errno to indicate a reason.
 */
extern int
match_find_best(struct match *match, const unsigned char matchv[MATCH_SIZE])
{
	unsigned long long locount, hicount;
	size_t i, j;
	tileset half, quarters[4];
	int max, hlo, hhi;

	tileset_unrank_init(6);
	tileset_unrank_init(12);

	memset(match, 0, sizeof *match);

	max = 0;
	for (i = 0; i < TWELVE_TILES / 2; i++) {
		half = tileset_unranknz(12, i);
		hlo = match_half_best(matchv, half, quarters, &locount);
		hhi = match_half_best(matchv, tileset_difference(NONZERO_TILES, half), quarters + 2, &hicount);
		assert(locount != 0);
		assert(hicount != 0);
		if (hlo + hhi > max) {
			max = hlo + hhi;
			match->count = 0;
		}

		if (hlo + hhi >= max) {
			match->count += locount * hicount;
			for (j = 0; j < 4; j++) {
				match->ts[j] = quarters[j];
				match->hval[j] = matchv[tileset_ranknz(quarters[j])];
			}
		}
	}

	/* each partitioning was tried twice, account for this in count */
	match->count /= 2;

	return (1);
}

/*
 * Try all ways to match the given half into two quarters.  Return the
 * highest h value found, store one partitioning with the highest
 * h value to quarters and the number of partitionings with that h value
 * to count.
 */
static int
match_half_best(const unsigned char matchv[MATCH_SIZE], tileset half,
    tileset quarters[2], unsigned long long *count)
{
	size_t i;
	tileset loquarter, hiquarter;
	int max = 0, hval;

	*count = 0;

	for (i = 0; i < SIX_OF_TWELVE / 2; i++) {
		loquarter = pdep(half, tileset_unrank(6, i));
		hval = matchv[tileset_ranknz(loquarter)];
		hiquarter = tileset_difference(half, loquarter);
		hval += matchv[tileset_ranknz(hiquarter)];
		if (hval > max) {
			max = hval;
			*count = 0;
		}

		if (hval >= max) {
			quarters[0] = loquarter;
			quarters[1] = hiquarter;
			++*count;
		}
	}

	return (max);
}
