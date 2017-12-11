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

static int	match_half_best(const unsigned char *, tileset);
static void	get_best_match(const unsigned char *matchv, struct match *match, tileset, int, int);

/*
 * Find the best way to partition the tray into 4 groups of six tiles
 * and store the optimal matches in match.  On success return 1,  on
 * error, return 0 and set errno to indicate a reason.
 */
extern int
match_find_best(struct match *match, const unsigned char *matchv)
{
	size_t i;
	tileset half;
	unsigned char (*halves)[2], max;

	tileset_unrank_init(6);
	tileset_unrank_init(12);

	halves = malloc(TWELVE_TILES);
	if (halves == NULL)
		return (0);

	/* find maximum matchings for halves */
	for (i = 0; i < TWELVE_TILES / 2; i++) {
		half = tileset_unrank(12, i) << 1;
		halves[i][0] = match_half_best(matchv, half);
		half = tileset_difference(NONZERO_TILES, half);
		prefetch(&unrank_tables[12][i + 1]);
		halves[i][1] = match_half_best(matchv, half);
	}

	/* find maximum matching value */
	max = 0;
	for (i = 0; i < TWELVE_TILES / 2; i++)
		if (halves[i][0] + halves[i][1] > max)
			max = halves[i][0] + halves[i][1];

	/* return maximum matchings */
	for (i = 0; i < TWELVE_TILES / 2; i++) {
		if (halves[i][0] + halves[i][1] != max)
			continue;

		half = tileset_unrank(12, i) << 1;
		get_best_match(matchv, match, half, halves[i][0], halves[i][1]);
		break;
	}

	free(halves);

	return (1);
}

/*
 * Try all ways to partition the given mask of twelve tiles into
 * two sets of six tiles and return the highest h value found.
 */
static int
match_half_best(const unsigned char *matchv, tileset half)
{
	size_t i;
	tileset quarter;
	int max = 0, hval;

	for (i = 0; i < SIX_OF_TWELVE / 2; i++) {
		quarter = pdep(half, tileset_unrank(6, i));
		hval = matchv[tileset_rank(quarter)];
		quarter = tileset_difference(half, quarter);
		hval += matchv[tileset_rank(quarter)];
		if (hval > max)
			max = hval;
	}

	return (max);
}

/*
 * Find a match with half the tiles in half, the h value for the tiles
 * in half being lohval and the h value for the remaining tiles being
 * hihval and store it in match.
 */
static void
get_best_match(const unsigned char *matchv, struct match *match,
	tileset half, int lohval, int hihval)
{
	size_t i;
	tileset quarter;
	int hval0, hval1;

	/* lower half */
	for (i = 0; i < SIX_OF_TWELVE / 2; i++) {
		quarter = pdep(half, tileset_unrank(6, i));
		hval0 = matchv[tileset_rank(quarter)];
		quarter = tileset_difference(half, quarter);
		hval1 = matchv[tileset_rank(quarter)];
		if (hval0 + hval1 == lohval)
			break;
	}

	assert(hval0 + hval1 == lohval);

	match->ts[0] = tileset_difference(half, quarter);
	match->ts[1] = quarter;

	match->hval[0] = hval0;
	match->hval[1] = hval1;

	/* upper half */
	half = tileset_difference(NONZERO_TILES, half);
	for (i = 0; i < SIX_OF_TWELVE / 2; i++) {
		quarter = pdep(half, tileset_unrank(6, i));
		hval0 = matchv[tileset_rank(quarter)];
		quarter = tileset_difference(half, quarter);
		hval1 = matchv[tileset_rank(quarter)];
		if (hval0 + hval1 == hihval)
			break;
	}

	assert(hval0 + hval1 == hihval);

	match->ts[2] = tileset_difference(half, quarter);
	match->ts[3] = quarter;

	match->hval[2] = hval0;
	match->hval[3] = hval1;
}
