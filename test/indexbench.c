/*-
 * Copyright (c) 2019 Robert Clausecker. All rights reserved.
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

/* indexbench.c -- benchmark the performance of compute_index() */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "puzzle.h"
#include "index.h"
#include "random.h"
#include "pdb.h"

/*
 * A catalogue of 16 PDBs @ 6 tiles as a benchmark.  These are basically
 * the PDBs from the small catalogue with two extra PDBs to get 16.
 * These tile sets do not contain the zero tile.  If a ZPDB benchmark is
 * run, the zero tile is added to each tile set.
 */
enum { TESTWIDTH = 16, NPUZZLE = 1000000, };
static tileset bench_ts[TESTWIDTH] = {
	/* from the small catalogue */
	0x00001c62, /* A: 1,5,6,10,11,12 */
	0x000001ce, /* B: 2,3,4,7,8,9 */
	0x00738000, /* C: 15,16,17,20,21,22 */
	0x018c6000, /* D: 13,14,18,19,23,24 */
	0x00000c66, /* E: 1,2,5,6,10,11 */
	0x00001398, /* F: 3,4,7,8,9,12 */
	0x0007e000, /* G: 13,14,15,16,17,18 */
	0x01f80000, /* H: 19,20,21,22,23,24 */
	0x00108462, /* I: 1,5,6,10,15, 20 */
	0x00631800, /* K: 11,12,16,17,21,22 */
	0x00843180, /* L: 7,8,12,13,18,23 */
	0x01084218, /* M: 3,4,9,14,19,24 */
	0x00073800, /* N: 11,12,13,16,17,18 */
	0x01e84000, /* O: 14,19,21,22,23,24 */

	/* from the large catalogue */
	0x00030846, /* P: 1,2,6,11,16,17 */
	0x00708420, /* Q: 5,10,15,20,21,22 */
};

/* test flags */
enum {
	WANT_LOOKUP = 1 << 0,
	WANT_ZPDB = 1 << 1,
	WANT_VECTORIZED = 1 << 2,
};

/*
 * Fill pdb with random values between 0 and 63 for testing.
 * Do not respect parity.
 */
static void
randomize(struct patterndb *pdb)
{
	uint64_t x;
	size_t i, n;
	int fill = 0;

	n = search_space_size(&pdb->aux);

	for (i = 0; i < n; i++) {
		if (fill <= 0) {
			x = xorshift();
			fill = 64 / 6;
		}

		pdb->data[i] = x & 64;
		x >>= 6;
		fill--;
	}
}

/*
 * Vectorised benchmark: same as dobench, but with vectors.
 */
static void
dovbench(struct patterndb **pdbs, const tileset *tilesets, size_t npdb,
    const struct puzzle *puzzles, size_t npuzzle, int flags)
{
	permindex pidxbuf[VECTORWIDTH];
	tsrank maprank[VECTORWIDTH];
	int h[VECTORWIDTH];
	const atomic_uchar *pdb_data[VECTORWIDTH];

	size_t i;

	if (flags & WANT_LOOKUP) {
		for (i = 0; i < npdb; i++)
			pdb_data[i] = pdbs[i]->data;
	}

	for (i = 0; i < npuzzle; i++) {
		compute_index_16a6(pidxbuf, maprank, puzzles + i, tilesets);

		if (flags & WANT_LOOKUP)
			pdb_lookup_16a6(h, pidxbuf, maprank, pdb_data);
	}

	/* TODO: implement ZPDBs */
}

/*
 * Benchmark: compute the indices for npuzzle puzzles in npdb pdbs.  If
 * flags & WANT_LOOKUP, also look the result up in the pdbs.
 */
static void
dobench(struct patterndb **pdbs, const tileset *tilesets, size_t npdb,
    const struct puzzle *puzzles, size_t npuzzle, int flags)
{
	struct index idx;
	size_t i, j;
	volatile int sink; /* prevent the compiler from optimising this away */
	int sum;

	if (flags & WANT_VECTORIZED) {
		dovbench(pdbs, tilesets, npdb, puzzles, npuzzle, flags);
		return;
	}

	for (i = 0; i < npuzzle; i++) {
		sum = 0;
		for (j = 0; j < npdb; j++) {
			compute_index(&pdbs[j]->aux, &idx, puzzles + i);
			if (flags & WANT_LOOKUP)
				sum += pdb_lookup(pdbs[j], &idx);
		}

		sink = sum;
	}
}

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-lvz] [runs]\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	struct timespec begin, end;
	struct patterndb *pdbs[TESTWIDTH];
	struct puzzle *puzzles;
	double fbegin, fend, dur;
	long j, runs = 10;
	size_t i;
	int optchar, flags = 0;

	while (optchar = getopt(argc, argv, "lvz"), optchar != -1)
		switch (optchar) {
		case 'z':
			flags |= WANT_ZPDB;
			break;

		case 'v':
			flags |= WANT_VECTORIZED;
			break;

		case 'l':
			flags |= WANT_LOOKUP;
			break;

		default:
			usage(argv[0]);
		}

	switch (argc - optind) {
	case 0:
		break;

	case 1:
		runs = atol(argv[optind]);
		break;

	default:
		usage(argv[0]);
	}

	if (flags & WANT_ZPDB)
		/* add tile 0 to all tile sets */
		for (i = 0; i < TESTWIDTH; i++)
			bench_ts[i] |= 1;

	if (flags & WANT_LOOKUP)
		/* allocate random PDBs */
		for (i = 0; i < TESTWIDTH; i++) {
			pdbs[i] = pdb_allocate(bench_ts[i]);
			if (pdbs[i] == NULL) {
				perror("pdb_allocate");
				return (EXIT_FAILURE);
			}

			randomize(pdbs[i]);
		}
	else
		/* allocate dummies */
		for (i = 0; i < TESTWIDTH; i++) {
			pdbs[i] = pdb_dummy(bench_ts[i]);
			if (pdbs[i] == NULL) {
				perror("pdb_dummy");
				return (EXIT_FAILURE);
			}
		}

	/* allocate and generate random puzzles */
	puzzles = malloc(NPUZZLE * sizeof *puzzles);
	if (puzzles == NULL) {
		perror("malloc");
		return (EXIT_FAILURE);
	}

	for (i = 0; i < NPUZZLE; i++)
		random_puzzle(puzzles + i);

	/* warm up round */
	dobench(pdbs, bench_ts, TESTWIDTH, puzzles, NPUZZLE, flags);

	clock_gettime(CLOCK_REALTIME, &begin);

	for (j = 0; j < runs; j++)
		dobench(pdbs, bench_ts, TESTWIDTH, puzzles, NPUZZLE, flags);

	clock_gettime(CLOCK_REALTIME, &end);

	fbegin = begin.tv_sec + begin.tv_nsec / 1e9;
	fend = end.tv_sec + end.tv_nsec / 1e9;
	dur = fend - fbegin;

	printf("%gs elapsed, %gs per lookup.\n", dur, dur / NPUZZLE / TESTWIDTH / runs);

	return (EXIT_SUCCESS);
}
