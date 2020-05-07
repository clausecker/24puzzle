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
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "catalogue.h"
#include "fsm.h"
#include "pdb.h"
#include "puzzle.h"
#include "search.h"
#include "tileset.h"
#include "transposition.h"

struct search_state {
	jmp_buf finish;
	struct pdb_catalogue *cat;
	const struct fsm *fsm;
	struct path *path;
	size_t bound;
	unsigned long long expanded, pruned;
	int n_solutions, flags;
	void (*on_solved)(const struct path *, void *);
	void *on_solved_payload;
};

/*
 * Expand the search tree for configuration p recursively.  Assume the
 * search path up to here has had length g already.  Use the search
 * state in sst.
 */
static void
expand_node(struct search_state *sst, size_t g, struct puzzle *p,
    struct fsm_state st, struct partial_hvals *ph)
{
	struct partial_hvals pph;
	struct fsm_state ast;
	size_t i, h, n_moves, zloc, dest, tile;
	const signed char *moves;

	h = catalogue_ph_hval(sst->cat, ph);
	if (h == 0 && memcmp(p->tiles, solved_puzzle.tiles, TILE_COUNT) == 0) {
		sst->n_solutions++;
		sst->path->pathlen = g;

		if (sst->flags & IDA_VERBOSE)
			fprintf(stderr, "Solution found at depth %zu\n", g);

		if (sst->on_solved != NULL)
			sst->on_solved(sst->path, sst->on_solved_payload);

		if (~sst->flags & IDA_LAST_FULL)
			longjmp(sst->finish, 1);

		return;
	}

	if (g + h > sst->bound)
		return;

	fsm_prefetch(sst->fsm, st);
	sst->expanded++;
	zloc = zero_location(p);
	moves = get_moves(zloc);
	n_moves = move_count(zloc);

	for (i = 0; i < n_moves; i++) {
		dest = moves[i];
		ast = fsm_advance_idx(sst->fsm, st, i);
		if (fsm_is_match(ast)) {
			sst->pruned++;
			continue;
		}

		sst->path->moves[g] = dest;

		tile = p->grid[dest];
		move(p, dest);
		pph = *ph;
		catalogue_diff_hvals(&pph, sst->cat, p, tile);
		expand_node(sst, g + 1, p, ast, &pph);
		move(p, zloc);
	}
}

/*
 * Use PDB catalogue cat and finite state machine fsm to search for a
 * solution for p with length bound.  Return the number of solutions found.
 * Update bound with the least bound needed to expand extra nodes.
 * Write the number of expanded nodes to expanded.  For each solution found,
 * if on_solved is not NULL call on_solved on the solution with
 * payload as the second argument
 */
static int
search_to_bound(struct path *path, struct pdb_catalogue *cat,
    const struct fsm *fsm, const struct puzzle *p, size_t bound,
    unsigned long long *expanded, void (*on_solved)(const struct path *,
    void *), void *payload, int flags) {
	struct partial_hvals ph;
	struct puzzle pp;
	struct search_state sst;
	struct fsm_state st;

	sst.cat = cat;
	sst.fsm = fsm;
	sst.path = path;
	sst.flags = flags;

	sst.n_solutions = 0;
	sst.expanded = 0;
	sst.pruned = 0;
	sst.bound = bound;
	sst.on_solved = on_solved;
	sst.on_solved_payload = payload;

	if (setjmp(sst.finish))
		goto finish;

	pp = *p; /* allow us to modify p */
	st = fsm_start_state(zero_location(&pp));
	catalogue_partial_hvals(&ph, sst.cat, &pp);

	expand_node((struct search_state *)&sst, 0, &pp, st, &ph);

finish:
	*expanded = sst.expanded;

	if (flags & IDA_VERBOSE)
		fprintf(stderr, "Finite state machine pruned %llu nodes in previous round.\n", sst.pruned);

	if (sst.n_solutions == 0)
		path->pathlen = SEARCH_NO_PATH;

	return (sst.n_solutions);
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
 * Verify that path indeed solves p.  Return 1 if it does, 0 otherwise.
 */
static int
verify(const struct puzzle *p, const struct path *path)
{
	struct puzzle pp;

	/* if no path was found, there's nothing to verify */
	if (path->pathlen == SEARCH_NO_PATH)
		return (1);

	pp = *p;
	path_walk(&pp, path);

	return (memcmp(solved_puzzle.tiles, pp.tiles, TILE_COUNT) == 0);
}

/*
 * Try to find a solution for parg wit the IDA* algorithm using the
 * disjoint pattern databases pdbs as heuristic functions and fsm as
 * a finite state machine to eliminate duplicate nodes.  If the
 * goal is more than limit steps away, abort the search and set
 * path->pathlen = SEARCH_NO_PATH.  Store the path found in path and
 * return the number of nodes expanded.  If f is not NULL, print
 * diagnostic messages to f.  If on_solved is not NULL, call on_solved
 * for each solution found with the solution and payload for arguments.
 */
extern unsigned long long
search_ida_bounded(struct pdb_catalogue *cat, const struct fsm *fsm,
    const struct puzzle *p, size_t limit, struct path *path,
    void (*on_solved)(const struct path *, void *), void *payload, int flags)
{
	struct timespec begin, round_begin, round_end, duration;
	unsigned long long expanded, total_expanded = 0;
	double dur;
	size_t bound;
	int n_solution = 0, no_clocks = 0;

	if (~flags & IDA_VERBOSE)
		no_clocks = 1;
	else if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &begin) != 0) {
		perror("clock_gettime");
		no_clocks = 1;
	} else
		round_end = begin;

	path->pathlen = SEARCH_NO_PATH;
	for (bound = catalogue_hval(cat, p); n_solution == 0 && bound <= limit; bound += 2) {
		if (flags & IDA_VERBOSE)
			fprintf(stderr, "Searching for solution with bound %zu\n", bound);

		n_solution = search_to_bound(path, cat, fsm, p, bound, &expanded, on_solved, payload, flags);
		total_expanded += expanded;

		if (flags & IDA_VERBOSE)
			fprintf(stderr, "Expanded %llu nodes during previous round.\n", expanded);

		if (no_clocks)
			continue;

		round_begin = round_end;

		if (no_clocks)
			continue;

		if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &round_end) != 0) {
			perror("clock_gettime");
			no_clocks = 1;
			continue;
		}

		duration = timediff(round_begin, round_end);
		dur = duration.tv_sec + duration.tv_nsec / 1000000000.0;
		fprintf(stderr, "Spent %.3f seconds computing the last round, %.2f nodes/s\n",
		    dur, expanded / dur);
	}

	if (flags & IDA_VERBOSE) {
		fprintf(stderr, "Expanded %llu nodes in total.\n", total_expanded);
		if (n_solution > 0)
			fprintf(stderr, "Found %d solution(s).\n", n_solution);
		else
			fprintf(stderr, "No solution found.\n");
	}

	if (!no_clocks) {
		duration = timediff(begin, round_end);
		dur = duration.tv_sec + duration.tv_nsec / 1000000000.0;
		fprintf(stderr, "Spent %.3f seconds in total, %.2f nodes/s\n",
		    dur, total_expanded / dur);
	}

	if (flags & IDA_VERIFY && !verify(p, path)) {
		if (flags & IDA_VERBOSE)
			fprintf(stderr, "Path incorrect!\n");

		abort();
	}

	return (total_expanded);
}

/*
 * Run search_ida_bounded but without a bound.
 */
extern unsigned long long
search_ida(struct pdb_catalogue *cat, const struct fsm *fsm,
    const struct puzzle *p, struct path *path,
    void (*on_solved)(const struct path *, void *), void *payload, int flags)
{
	return (search_ida_bounded(cat, fsm, p, SEARCH_PATH_LEN, path, on_solved, payload, flags));
}
