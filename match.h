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

#ifndef MATCH_H
#define MATCH_H

#include <stdio.h>

#include "tileset.h"
#include "heuristic.h"
#include "puzzle.h"

enum {
	/*
	 * The size of a match vector containing one h value for each
	 * way to take 6 tiles out of the 24 in the tray.
	 */
	MATCH_SIZE = 134596,
};

/*
 * A match records a way to partition the 24 tiles in the tray into
 * 4 groups of 6 tiles.  By incrementally looking up configurations in
 * various pattern databases with match_amend, a vector of partial
 * h values can be created.  Using this vector, an optimal partitioning
 * can be determined with match_find_best.  Similarly, a pessimal
 * partitioning can be determined with match_find_worst.
 */
struct match {
	tileset ts[4];
	unsigned char hval[4];
};

extern int	match_find_best(struct match *, const unsigned char *);
//extern size_t	match_find_worst(struct match *, size_t, const unsigned char *);

/*
 * Allocate a match vector.  This function wraps malloc() for convenience.
 */
static inline unsigned char *
matchv_allocate(void)
{
	return (calloc(MATCH_SIZE, 1));
}

/*
 * Release a match vector.  This function wraps free() for convenience.
 */
static inline void
matchv_free(unsigned char *v)
{
	free(v);
}

/*
 * Add the value predicted by heuristic h to match vector v.
 */
static inline void
match_amend(unsigned char *v, const struct puzzle *p, struct heuristic *heu)
{
	assert(!tileset_has(heu->ts, ZERO_TILE));
	assert(tileset_count(heu->ts) == 6);
	v[tileset_rank(heu->ts)] = heu_hval(heu, p);
}

#endif /* MATCH_H */
