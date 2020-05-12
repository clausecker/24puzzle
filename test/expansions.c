/*-
 * Copyright (c) 2020 Robert Clausecker. All rights reserved.
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

/* expansions.c -- measure IDA* node expansions for random problems */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>
#include <pthread.h>

#include "catalogue.h"
#include "fsm.h"
#include "random.h"
#include "pdb.h"

struct pdb_catalogue *cat = NULL;
const struct fsm *fsm = &fsm_simple;
static long long n_puzzle = 1000; /* number of puzzles to sample */
static atomic_llong n_done = 0; /* number of puzzles sampled */
static int limit = 60; /* search this deep */
static atomic_llong histogram[PDB_HISTOGRAM_LEN]; /* expanded nodes histogram */

/*
 * Perform an IDA* round to depth d on puzzle p.  This implementation of
 * IDA* is modified compared to search_ida() in that we don't care if we
 * find the goal and don't record the path we travelled.  Thus, a
 * special implementation is used.
 *
 * TODO: somehow integrate this into the main IDA code.
 */
static long long
expand(struct puzzle *p, int d, struct fsm_state st, struct partial_hvals *ph)
{
	struct partial_hvals aph;
	struct fsm_state ast;
	long long x = 0;
	size_t i, n_moves;
	int zloc, h, dest, tile;
	const signed char *moves;

	h = catalogue_ph_hval(cat, ph);
	if (h > d)
		return (0);
	else
		x++;

	fsm_prefetch(fsm, st);
	zloc = zero_location(p);
	moves = get_moves(zloc);
	n_moves = move_count(zloc);

	for (i = 0; i < n_moves; i++) {
		dest = moves[i];
		ast = fsm_advance_idx(fsm, st, i);
		if (fsm_is_match(ast))
			continue;

		tile = p->grid[dest];
		move(p, dest);
		aph = *ph;
		catalogue_diff_hvals(&aph, cat, p, tile);
		x += expand(p, d - 1, ast, &aph);
		move(p, zloc);
	}

	return (x);
}

/*
 * Pick puzzles from n_done and perform IDA* rounds up to limit on them.
 * Print the results to stdout and create a histogram.
 */
static void *
take_expansions(void *unused)
{
	struct puzzle p;
	struct partial_hvals ph;
	struct fsm_state st;
	long long n, x;
	int d;

	(void)unused;

	while (n = n_done++, n < n_puzzle) {
		/* TODO: make this deterministic in concurrent operation */
		random_puzzle(&p);

		st = fsm_start_state(zero_location(&p));
		catalogue_partial_hvals(&ph, cat, &p);
		for (d = catalogue_ph_hval(cat, &ph); d <= limit; d++) {
			x = expand(&p, d, st, &ph);
			printf("%lld,%d,%lld\n", n, d, x);
			histogram[d] += x;
		}
	}

	return (NULL);
}

/*
 * Start pdb_jobs threads computing expansions.  When they are done, collect them
 * and return.
 */
static void
start_threads(void)
{
	pthread_t pool[PDB_MAX_JOBS];

	int i, jobs, error;

	jobs = pdb_jobs;

	/* for easier debugging, don't multithread when jobs == 1 */
	if (jobs == 1) {
		take_expansions(NULL);
		return;
	}

	for (i = 0; i < jobs; i++) {
		error = pthread_create(pool + i, NULL, take_expansions, NULL);
		if (error == 0)
			continue;

		fprintf(stderr, "pthread_create: %s\n", strerror(error));

		if (i++ > 0)
			break;

		fprintf(stderr, "Couldn't create any threads, aborting...\n");
		exit(EXIT_FAILURE);
	}

	jobs = i;

	/* collect threads */
	for (i = 0; i < jobs; i++) {
		error = pthread_join(pool[i], NULL);
		if (error == 0)
			continue;

		fprintf(stderr, "pthread_join: %s\n", strerror(error));
	}
}

/*
 * evaluate the histogram.
 */
static void
evaluation(void)
{
	double n;
	int i;

	n = (double)n_puzzle;
	for (i = 0; i <= limit; i++)
		printf("avg,%d,%f\n", i, histogram[i] / n);
}

static void
usage(char *argv0)
{
	fprintf(stderr, "Usage: %s [-v] [-l limit] [-m fsm] [-j nproc] [-d pdbdir]"
	    " [-n npuzzle] [-s seed] catalogue\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	FILE *fsmfile;
	int optchar, verbose = 0;
	char *pdbdir = NULL;

	while (optchar = getopt(argc, argv, "d:j:l:m:n:s:v"), optchar != -1)
		switch (optchar) {
		case 'd':
			pdbdir = optarg;
			break;

		case 'j':
			pdb_jobs = atoi(optarg);
			if (pdb_jobs < 1 || pdb_jobs > PDB_MAX_JOBS) {
				fprintf(stderr, "Number of threads must be between 1 and %d\n",
				    PDB_MAX_JOBS);
				return (EXIT_FAILURE);
			}

			break;

		case 'l':
			limit = atoi(optarg);
			break;

		case 'm':
			fsmfile = fopen(optarg, "rb");
			if (fsmfile == NULL) {
				perror(optarg);
				return (EXIT_FAILURE);
			}

			fsm = fsm_load(fsmfile);
			if (fsm == NULL)  {
				perror("fsm_load");
				return (EXIT_FAILURE);
			}

			fclose(fsmfile);
			break;

		case 'n':
			n_puzzle = atoll(optarg);
			if (n_puzzle <= 0) {
				fprintf(stderr, "Number of puzzles must be positive: %s\n", optarg);
				return (EXIT_FAILURE);
			}

			break;

		case 's':
			set_seed(atoll(optarg));
			break;

		case 'v':
			verbose = 1;
			break;

		default:
			usage(argv[0]);
		}

	if (argc != optind + 1)
		usage(argv[0]);

	cat = catalogue_load(argv[optind], pdbdir, 0, verbose ? stderr : NULL);
	if (cat == NULL) {
		perror("catalogue_load");
		return (EXIT_FAILURE);
	}

	start_threads();
	evaluation();

	return (EXIT_SUCCESS);
}
