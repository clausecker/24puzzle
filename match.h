/*-
 * Copyright (c) 2017--2018 Robert Clausecker. All rights reserved.
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

#ifndef MATCH_H
#define MATCH_H

#include <assert.h>
#include <string.h>

#include "tileset.h"
#include "heuristic.h"
#include "puzzle.h"
#include "pdb.h"

enum {
	/*
	 * The size of a match vector containing one h value for each
	 * way to take 6 tiles out of the 24 in the tray.
	 */
	MATCH_SIZE = 134596,
};

/* scaling factor for the quality vector */
#define	QUALITY_SCALE (25ULL * 24 * 23 * 22 * 21 * 20 * 19)

/*
 * A match records a way to partition the 24 tiles in the tray into
 * 4 groups of 6 tiles.  By incrementally looking up configurations in
 * various pattern databases with match_amend, a vector of partial
 * h values can be created.  Using this vector, an optimal partitioning
 * can be determined with match_find_best.  Similarly, a pessimal
 * partitioning can be determined with match_find_worst.  The member
 * count contains the number of matches with this h value, quality
 * contains the quality of the partitioning found.
 */
struct match {
	tileset ts[4];
	unsigned char hval[4];
	unsigned long long count;
	unsigned long long quality;
};

/*
 * A struct quality contains for one PDB the qualities of its entries.
 * The quality is measured by the entries' average h values and by the
 * PDB's eta value.  Four h-averages can be summed to form the h-average
 * for a PDB set.  Four partial etas can be multiplied to form a "fake
 * eta" for the PDB set, a number that allows for a ranking of PDBs that
 * closely matches the ranking by true eta.
 */
struct quality {
	unsigned long long havg;
	double peta;
};

extern struct quality *qualities_load(const char *);
extern int	match_find_best(struct match *, const unsigned char[MATCH_SIZE], const struct quality[MATCH_SIZE]);
//extern size_t	match_find_worst(struct match *, size_t, const unsigned char *);

/*
 * Allocate a match vector.  This function wraps malloc() for convenience.
 */
static inline unsigned char *
matchv_allocate(void)
{
	unsigned char *v = malloc(MATCH_SIZE);

	if (v != NULL)
		memset(v, UNREACHED, MATCH_SIZE);

	return (v);
}

/*
 * Release a match vector.  This function wraps free() for convenience.
 */
static inline void
matchv_free(unsigned char v[MATCH_SIZE])
{

	free(v);
}

/*
 * Add the value predicted by heuristic h to match vector v.
 */
static inline void
match_amend(unsigned char v[MATCH_SIZE], const struct puzzle *p, struct heuristic *heu)
{
	tsrank heurank = tileset_rank(heu->ts >> 1);

	assert(!tileset_has(heu->ts, ZERO_TILE));
	assert(tileset_count(heu->ts) == 6);
	assert(heurank < MATCH_SIZE);

	v[heurank] = heu_hval(heu, p);
}

/*
 * Make sure all entries in the match vector have been filled in.
 * Return zero if any entry remains UNREACHED, nonzero otherwise.
 */
static inline int
match_all_filled_in(const unsigned char v[MATCH_SIZE])
{
	size_t i;

	for (i = 0; i < MATCH_SIZE; i++)
		if (v[i] == UNREACHED)
			return (0);

	return (1);
}

/*
 * Allocate a quality vector of all zeroes, effectively ignoring the
 * maximum quality behaviour.
 */
static inline struct quality *
qualities_dummy(void)
{

	return (calloc(MATCH_SIZE, sizeof (struct quality)));
}

/*
 * Release a quality vector.  This is just a thin wrapper around free().
 */
static inline void
qualities_free(struct quality qualities[MATCH_SIZE])
{

	free(qualities);
}

/*
 * Given a quality vector, return the quality of the given tile set.
 * The tileset must contain exactly six tiles.
 */
static inline const struct quality *
get_quality(const struct quality qualities[MATCH_SIZE], tileset ts)
{

	assert(tileset_count(tileset_remove(ts, ZERO_TILE)) == 6);

	return (qualities + tileset_ranknz(ts));
}

/*
 * Convert a heuristic quality into an average h value.
 */
static inline double
quality_to_hval(unsigned long long q)
{

	return ((double)q / QUALITY_SCALE);
}

#endif /* MATCH_H */
