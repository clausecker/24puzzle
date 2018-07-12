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

/*
 * puzzledistext.c -- compute the number of puzzles with the given
 * distance using an external radix sort
 */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <unistd.h>

#include "compact.h"
#include "puzzle.h"
#include "random.h"

/*
 * The number of legal puzzle configurations, i.e. 25! / 2.  This
 * number is provided as a string, too, so we can generate fancier
 * output.
 */
#define CONFCOUNT 7755605021665492992000000.0
#define CONFCOUNTSTR "7755605021665492992000000"

/*
 * Read a single struct compact_puzzle from cpfile and write it to cp.
 * Return EOF on EOF, 0 otherwise.  Terminate the program on error.
 */
static int
getpuzzle(struct compact_puzzle *cp, FILE *cpfile)
{
	size_t count;

	count = fread(cp, sizeof *cp, 1, cpfile);
	if (count == 1)
		return (0);

	if (ferror(cpfile)) {
		perror("getpuzzle");
		exit(EXIT_FAILURE);
	}

	return (EOF);
}

/*
 * Write a single struct compact_puzzle from cp to cpfile.
 * Terminate the program on error.
 */
static void
putpuzzle(FILE *cpfile, const struct compact_puzzle *cp)
{
	size_t count;

	count = fwrite(cp, sizeof *cp, 1, cpfile);
	if (count != 1) {
		perror("putpuzzle");
		exit(EXIT_FAILURE);
	}
}

/*
 * Perform all unmasked moves from cp and write them to cpfile.
 */
static void
expand(FILE *cpfile, const struct compact_puzzle *cp)
{
	struct puzzle p;
	struct compact_puzzle ncp[4];
	size_t i, j, n_move, count;
	int movemask = move_mask(cp), zloc;
	const signed char *moves;

	unpack_puzzle(&p, cp);
	zloc = zero_location(&p);
	n_move = move_count(zloc);
	moves = get_moves(zloc);

	for (i = j = 0; i < n_move; i++) {
		if (movemask & 1 << i)
			continue;

		move(&p, moves[i]);
		pack_puzzle_masked(ncp + j++, &p, zloc);
		move(&p, zloc);
	}

	count = fwrite(ncp, sizeof *ncp, j, cpfile);
	if (count != j) {
		perror("expand");
		exit(EXIT_FAILURE);
	}
}

/*
 * Coalesce identical puzzles from infile, oring their move masks.
 * Write the resulting puzzles to outfile.
 */
static void
coalesce(FILE *outfile, FILE *infile)
{
	struct compact_puzzle a, b;
	size_t i, j, count;

	if (getpuzzle(&a, infile) == EOF)
		return;

	while (getpuzzle(&b, infile) != EOF) {
		if (a.hi == b.hi && ((a.lo ^ b.lo) & ~MOVE_MASK) == 0)
			a.lo |= b.lo;
		else {
			putpuzzle(outfile, &a);
			a = b;
		}
	}

	putpuzzle(outfile, &a);
}

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-l limit] [-f filename] [-n n_samples] [-s seed] shuffledir\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	struct cp_slice old_cps, new_cps;
	struct compact_puzzle cp;
	int optchar, i, limit = INT_MAX;
	size_t n_samples = 1 << 20;
	const char *samplefile = NULL, *shuffledir;

	while (optchar = getopt(argc, argv, "f:l:n:s:"), optchar != -1)
		switch (optchar) {
		case 'f':
			samplefile = optarg;
			break;

		case 'l':
			limit = atoi(optarg);
			break;

		case 'n':
			n_samples = strtoull(optarg, NULL, 0);
			break;

		case 's':
			random_seed = strtoull(optarg, NULL, 0);
			break;

		default:
			usage(argv[0]);
			break;
		}

	if (argc != optind + 1)
		usage(argv[0]);

	shuffledir = argv[optind];

	cps_init(&new_cps);
	pack_puzzle(&cp, &solved_puzzle);
	cps_append(&new_cps, &cp);

	if (samplefile != NULL)
		do_sampling(samplefile, &new_cps, 0, n_samples);

	/* keep format compatible with samplegen */
	printf("%s\n\n", CONFCOUNTSTR);

	printf("%3d: %18zu/%s = %24.18e\n", 0,
	    new_cps.len, CONFCOUNTSTR, new_cps.len / CONFCOUNT);

	for (i = 1; i <= limit; i++) {

		fflush(stdout);

		old_cps = new_cps;
		cps_init(&new_cps);

		cps_round(&new_cps, &old_cps);
		if (samplefile != NULL)
			do_sampling(samplefile, &new_cps, i, n_samples);

		cps_free(&old_cps);

		printf("%3d: %18zu/%s = %24.18e\n", i,
		    new_cps.len, CONFCOUNTSTR, new_cps.len / CONFCOUNT);
	}
}
