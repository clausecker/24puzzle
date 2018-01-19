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

/* walkdist.c -- estimate distance distributions from random walks */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "search.h"
#include "catalogue.h"
#include "pdb.h"
#include "index.h"
#include "puzzle.h"
#include "tileset.h"
#include "random.h"

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-i] [-j nproc] [-d pdbdir] [-n n_puzzle] [-s seed] catalogue distance\n", argv0);

	exit(EXIT_FAILURE);
}

/*
 * Perform an n step random walk from p, ignoring moves that immediately
 * undo the previous move.
 */
static void
random_walk(struct puzzle *p, int steps)
{
	unsigned long long lseed, entropy;
	size_t n_move, dloc;
	int i, zloc, prevtile, reservoir;
	const signed char *moves;

	if (steps == 0)
		return;

	/*
	 * In each iteration, we take log2(6) = 2.585 bits of
	 * entropy from entropy, carefully ensuring that our choice is
	 * unbiased.  A 64 bit integer has space for 24 such selections
	 * but we only take out 22 of them to keep the amount of
	 * rejected samples as low as possible.  reservoir keeps track
	 * of the amount of bits left in entropy until we need to sample
	 * again from lseed.
	 */
	do lseed = xorshift();
	while (lseed >= 18427038537917399040ull); /* 140 * 6 ** 22 */

	entropy = lseed;
	reservoir = 22;

	/*
	 * we assume that we start our walk from the solved
	 * configuration, so two possible moves exist initially.
	 */
	zloc = zero_location(p);
	assert(zloc == 0);
	n_move = 2;
	dloc = entropy % 2;
	entropy /= 6;
	reservoir--;

	prevtile = zloc;
	move(p, get_moves(zloc)[dloc]);

	for (i = 1; i < steps; i++) {
		if (reservoir == 0) {
			do lseed = xorshift_step(lseed);
			while (lseed >= 18427038537917399040ull);

			entropy = lseed;
			reservoir = 22;
		}

		zloc = zero_location(p);
		moves = get_moves(zloc);

		switch (move_count(zloc)) {
		case 2:
			dloc = moves[0] == prevtile ? moves[1] : moves[0];
			break;

		case 3:
			dloc = entropy % 2;
			dloc = moves[dloc] == prevtile ? moves[dloc + 1] : moves[dloc];
			entropy /= 6;
			reservoir--;
			break;

		case 4:
			dloc = entropy % 3;
			dloc = moves[dloc] == prevtile ? moves[dloc + 1] : moves[dloc];
			entropy /= 6;
			reservoir--;
			break;

		default:
			/* UNREACHABLE */
			assert(0);
		}

		prevtile = zloc;
		move(p, dloc);
	}
}

static void
collect_walks(size_t samples[SEARCH_PATH_LEN], int steps, size_t n_puzzle,
    struct pdb_catalogue *cat, int give_heu)
{
	struct puzzle p;
	struct path path;
	size_t i;

	for (i = 0; i < n_puzzle; i++) {
		p = solved_puzzle;
		random_walk(&p, steps);
		if (give_heu)
			samples[catalogue_hval(cat, &p)]++;
		else {
			search_ida(cat, &p, &path, NULL);
			assert(path.pathlen != SEARCH_NO_PATH);
			samples[path.pathlen]++;
		}
	}
}

static void
print_statistics(size_t samples[SEARCH_PATH_LEN], int steps, size_t n_puzzle)
{
	double n_quot = 1.0 / n_puzzle;
	size_t i, len = 0;

	printf("%d %zu", steps, n_puzzle);

	/* find last nonzero entry in samples */
	for (i = 0; i < SEARCH_PATH_LEN; i++)
		if (samples[i] != 0)
			len = i + 1;

	for (i = 0; i < len; i++)
		printf(" %g", samples[i] * n_quot);

	printf("\n");
}

extern int
main(int argc, char *argv[])
{
	struct pdb_catalogue *cat;
	size_t samples[SEARCH_PATH_LEN], n_puzzle = 1000;
	int optchar, catflags = 0, give_heu = 0, steps;
	char *pdbdir = NULL;

	while (optchar = getopt(argc, argv, "d:ij:hn:s:"), optchar != -1)
		switch (optchar) {
		case 'd':
			pdbdir = optarg;
			break;

		case 'i':
			catflags |= CAT_IDENTIFY;
			break;

		case 'j':
			pdb_jobs = atoi(optarg);
			if (pdb_jobs < 1 || pdb_jobs > PDB_MAX_JOBS) {
				fprintf(stderr, "Number of threads must be between 1 and %d\n",
				    PDB_MAX_JOBS);
				return (EXIT_FAILURE);
			}

			break;

		case 'h':
			give_heu = 1;
			break;

		case 'n':
			n_puzzle = strtol(optarg, NULL, 0);
			break;

		case 's':
			random_seed = strtoll(optarg, NULL, 0);
			break;

		default:
			usage(argv[0]);
		}

	if (argc != optind + 2)
		usage(argv[0]);

	steps = (int)strtol(argv[optind + 1], NULL, 0);
	if (steps < 0) {
		fprintf(stderr, "Number of steps cannot be negative: %s\n", argv[optind + 1]);
		usage(argv[0]);
	}

	cat = catalogue_load(argv[optind], pdbdir, catflags, stderr);
	if (cat == NULL) {
		perror("catalogue_load");
		return (EXIT_FAILURE);
	}

	memset(samples, 0, sizeof samples);

	collect_walks(samples, steps, n_puzzle, cat, give_heu);
	print_statistics(samples, steps, n_puzzle);

	return (EXIT_SUCCESS);
}
