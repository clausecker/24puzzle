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

/* ida.c -- the IDA* algorithm */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "catalogue.h"
#include "pdb.h"
#include "puzzle.h"
#include "search.h"
#include "tileset.h"
#include "transposition.h"

/*
 * The search path as we store it while searching.  This
 * layout contains the following parts:
 *
 * - zloc is the location of the zero tile
 * - childno indicates what number child we are of our parent
 * - to_expand is a bitmap containg the nodes we need to expand from here.
 * - child_ph stores our children's partial node evaluations
 * - child_h stores our childrens node evaluations.
 *
 * The path begins with two dummy nodes to simplify the code that
 * excludes useless moves.
 */
struct search_node {
	struct partial_hvals child_ph[4];
	unsigned zloc, childno, to_expand, child_h[4];
};

/*
 * Fill in child_ph, child_h, and to_expand in child.  Omit
 * the node that would go back from the expansion.
 */
static void
evaluate_expansions(struct search_node *path, struct puzzle *p,
    struct pdb_catalogue *cat, int flags)
{
	size_t i, dest, tile;
	const signed char *moves;

	path->to_expand = 0;
	assert(path->zloc < TILE_COUNT);
	moves = get_moves(path->zloc);

	for (i = 0; i < 4; i++) {
		dest = moves[i];

		/* don't attempt to go back */
		if (dest == -1 || dest == path[-1].zloc)
			continue;

		path->to_expand |= 1 << i;

		tile = p->grid[dest];

		move(p, dest);
		memcpy(path->child_ph + i, path[-1].child_ph + path->childno, sizeof path->child_ph[i]);
		path->child_h[i] = catalogue_diff_hvals(path->child_ph + i, cat, p, tile);
		move(p, path->zloc);
	}
}

/*
 * Search for a solution for p through the search space.  Do not exceed
 * a total distance of *bound.  Store the number of nodes expanded in
 * *expanded.  Return 0 if a solution was found, -1 if not.  Print
 * diagnostic messages to f if f is not NULL.
 */
static int
search_to_bound(struct pdb_catalogue *cat, const struct puzzle *parg,
    struct search_node path[SEARCH_PATH_LEN + 2], size_t *bound, FILE *f,
    unsigned long long *expanded, int flags)
{
	struct puzzle p = *parg;
	size_t newbound = -1, dloc;
	int dist, done = -1;
	unsigned h, hmax, dest;

	/* initialize the dummy nodes */
	path[0].zloc = zero_location(&p); /* dummy value */
	path[0].childno = -1; /* dummy value */
	path[0].to_expand = 0; /* dummy value */
	h = catalogue_partial_hvals(&path[0].child_ph[0], cat, &p);
	path[0].child_h[0] = h;

	/*
	 * for easier programming, we want path[0] to be the root node,
	 * not the first dummy node, so we make path point right at it.
	 */
	path += 1;
	dist = 0;

	path[0].zloc = zero_location(&p);
	path[0].childno = 0;
	evaluate_expansions(path, &p, cat, flags);

	/* do graph search bounded by bound */
	do {
		hmax = path[dist - 1].child_h[path[dist].childno];

		/* are we out of moves to make? */
		if (path[dist].to_expand == 0) {
			dist--;
			move(&p, path[dist].zloc);
		} else { /* make the next move */
			++*expanded;

			dest = ctz(path[dist].to_expand);
			hmax = path[dist].child_h[dest];
			path[dist].to_expand &= ~(1 << dest);
			dloc = get_moves(path[dist].zloc)[dest];
			dist++;

			/* would we go over the limit? */
			if (hmax + dist > *bound) {
				if (hmax + dist < newbound)
					newbound = hmax + dist;

				dist--;
				continue;
			}

			move(&p, dloc);

			path[dist].childno = dest;
			path[dist].zloc = dloc;
			evaluate_expansions(path + dist, &p, cat, flags);

			/* have we found the solution? */
			if (hmax == 0 && memcmp(p.tiles, solved_puzzle.tiles, TILE_COUNT) == 0) {
				if (f != NULL)
					fprintf(f, "Solution found at depth %d.\n", dist);

				*bound = dist;

				if (flags & IDA_LAST_FULL)
					done = 0;
				else
					return (0);
			}
		}
	} while (dist >= 0); /* loop ends when we finish expanding the first node */

	if (f != NULL)
		fprintf(f, "No solution found with bound %zu, increasing bound to %zu.\n",
		    *bound, newbound);

	assert(newbound != -1);
	*bound = newbound;

	return (done);
}

/*
 * Compute the difference between two struct timespec and
 * return it.
 */
static struct timespec
timediff(struct timespec begin, struct timespec end)
{
	struct timespec result;

	if (end.tv_nsec < begin.tv_nsec) {
		result.tv_sec = end.tv_sec - begin.tv_sec - 1;
		result.tv_nsec = end.tv_nsec - begin.tv_nsec + 1000000000;
	} else {
		result.tv_sec = end.tv_sec - begin.tv_sec;
		result.tv_nsec = end.tv_nsec - begin.tv_nsec;
	}

	return (result);
}

/*
 * Try to find a solution for parg wit the IDA* algorithm using the
 * disjoint pattern databases pdbs as heuristic functions.  If the
 * goal is more than limit steps away, abort the search and set
 * path->pathlen = SEARCH_NO_PATH.  Store the path found in path and
 * return the number of nodes expanded.  If f is not NULL, print
 * diagnostic messages to f.
 */
extern unsigned long long
search_ida_bounded(struct pdb_catalogue *cat, const struct puzzle *p,
    size_t limit, struct path *path, FILE *f, int flags)
{
	struct search_node *spath;
	struct timespec begin, round_begin, round_end, duration;
	unsigned long long expanded, total_expanded = 0;
	double dur;
	size_t i, bound = 0;
	int unfinished, no_clocks = 0;

	/*
	 * the code doesn't work correctly when used on the solved
	 * configuration directly, so test for this situation and
	 * return an empty path.
	 */
	if (memcmp(p->tiles, solved_puzzle.tiles, TILE_COUNT) == 0) {
		path->pathlen = 0;
		return (0);
	}

	spath = malloc(sizeof *spath * (SEARCH_PATH_LEN + 2));
	if (spath == NULL) {
		perror("malloc");
		abort();
	}

	if (f == NULL)
		no_clocks = 1;

	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &begin) != 0) {
		fprintf(f, "clock_gettime: %s\n", strerror(errno));
		no_clocks = 1;
	} else
		round_end = begin;

	do {
		expanded = 0;
		unfinished = search_to_bound(cat, p, spath, &bound, f, &expanded, flags);
		total_expanded += expanded;

		if (f != NULL)
			fprintf(f, "Expanded %llu nodes during previous round.\n", expanded);

		if (no_clocks)
			continue;

		round_begin = round_end;

		if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &round_end) != 0) {
			fprintf(f, "clock_gettime: %s\n", strerror(errno));
			no_clocks = 1;
			continue;
		}

		duration = timediff(round_begin, round_end);
		dur = duration.tv_sec + duration.tv_nsec / 1000000000.0;
		fprintf(f, "Spent %.3f seconds computing the last round, %.2f nodes/s\n",
		    dur, expanded / dur);
	} while (unfinished && bound <= limit);

	if (f != NULL)
		fprintf(f, "Expanded %llu nodes in total.\n", total_expanded);

	if (!no_clocks) {
		duration = timediff(begin, round_end);
		dur = duration.tv_sec + duration.tv_nsec / 1000000000.0;
		fprintf(f, "Spent %.3f seconds in total, %.2f nodes/s\n",
		    dur, total_expanded / dur);
	}

	if (bound <= limit) {
		/* copy spath to path */
		path->pathlen = bound;
		for (i = 0; i < bound; i++)
			path->moves[i] = spath[i + 2].zloc;
	} else
		path->pathlen = SEARCH_NO_PATH;

	free(spath);

	return (total_expanded);
}

/*
 * Run search_ida_bounded but without a bound.
 */
extern unsigned long long
search_ida(struct pdb_catalogue *cat, const struct puzzle *p,
    struct path *path, FILE *f, int flags)
{
	return (search_ida_bounded(cat, p, SEARCH_PATH_LEN, path, f, flags));
}
