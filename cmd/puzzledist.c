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

/* puzzledist.c -- compute the number of puzzles with the given distance */

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
 * An array of struct compact_puzzle with the given length and capacity.
 */
struct cp_slice {
	struct compact_puzzle *data;
	size_t len, cap;
};

/*
 * Append cp to slice cps and resize if required.
 */
static void
cps_append(struct cp_slice *cps, const struct compact_puzzle *cp)
{
	struct compact_puzzle *newdata;
	size_t newcap;

	if (cps->len >= cps->cap) {
		newcap = cps->cap < 64 ? 64 : cps->cap * 13 / 8;
		newdata = realloc(cps->data, newcap * sizeof *cps->data);
		if (newdata == NULL) {
			perror("realloc");
			exit(EXIT_FAILURE);
		}

		cps->data = newdata;
		cps->cap = newcap;
	}

	cps->data[cps->len++] = *cp;
}

/*
 * Initialize the content of cps to an empty slice.
 */
static void
cps_init(struct cp_slice *cps)
{
	cps->data = NULL;
	cps->len = 0;
	cps->cap = 0;
}

/*
 * Release all storage associated with cps.  The content of cps is
 * undefined afterwards.
 */
static void
cps_free(struct cp_slice *cps)
{
	free(cps->data);
}

/*
 * Perform all unmasked moves from cp and add them to cps.
 */
static void
expand(struct cp_slice *cps, const struct compact_puzzle *cp)
{
	struct puzzle p;
	struct compact_puzzle ncp;
	size_t i, n_move;
	int movemask = cp->lo & MOVE_MASK, zloc;
	const signed char *moves;

	unpack_puzzle(&p, cp);
	zloc = zero_location(&p);
	n_move = move_count(zloc);
	moves = get_moves(zloc);

	for (i = 0; i < n_move; i++) {
		if (movemask & 1 << i)
			continue;

		move(&p, moves[i]);
		pack_puzzle_masked(&ncp, &p, zloc);
		move(&p, zloc);

		cps_append(cps, &ncp);
	}
}

/*
 * Given a sorted struct cp_slice cps, coalesce identical puzzles,
 * oring their move masks.  Since the move mask bits are the least
 * significatn bits in lo, puzzles differing only by their move
 * masks end up next to each other.
 */
static void
coalesce(struct cp_slice *cps)
{
	struct compact_puzzle *a, *b, *newdata;
	size_t i, j;

	if (cps->len == 0)
		return;

	/* invariant: i < j */
	for (i = 0, j = 1; j < cps->len; j++) {
		a = &cps->data[i];
		b = &cps->data[j];

		/* are a and b equal, ignoring the move mask? */
		if (a->hi == b->hi && ((a->lo ^ b->lo) & ~MOVE_MASK) == 0)
			a->lo |= b->lo;
		else
			cps->data[++i] = *b;
	}

	cps->len = i + 1;

	/* conserve storage */
	newdata = realloc(cps->data, cps->len * sizeof *cps->data);
	if (newdata != NULL) {
		cps->data = newdata;
		cps->cap = cps->len;
	}
}

/*
 * Expand vertices in cps and store them in new_cps.  Then sort and
 * coalesce new_cps.
 */
static void
do_round(struct cp_slice *restrict new_cps, const struct cp_slice *restrict cps)
{
	size_t i;

	for (i = 0; i < cps->len; i++)
		expand(new_cps, &cps->data[i]);

	qsort(new_cps->data, new_cps->len, sizeof *new_cps->data, compare_cp);
	coalesce(new_cps);
}

/*
 * Use samplefile as a prefix for a file name of the form %s.%d suffixed
 * with round and store up to n_samples randomly selected samples from
 * cps in it.  The samples are stored as struct cps with the move bits
 * undefined.  On error, report the error, discard the sample file, and
 * then continue.  The ordering of cps is destroyed in the process.
 */
static void
do_sampling(const char *samplefile, struct cp_slice *cps, int round, size_t n_samples)
{
	FILE *f;
	struct compact_puzzle tmp;
	size_t i, j, count;
	char pathbuf[PATH_MAX];

	snprintf(pathbuf, PATH_MAX, "%s.%d", samplefile, round);
	f = fopen(pathbuf, "wb");
	if (f == NULL) {
		perror(pathbuf);
		return;
	}

	/*
	 * if we have enough space, write all samples.  Otherwise, use a
	 * partial Fisher-Yates shuffle to generate samples.  Sort
	 * samples before writing for better cache locality in
	 * PDB lookup.
	 */
	if (n_samples >= cps->len)
		n_samples = cps->len;
	else {
		// TODO eliminate modulo bias
		for (i = 0; i < n_samples; i++) {
			j = i + xorshift() % (cps->len - i);
			tmp = cps->data[i];
			cps->data[i] = cps->data[j];
			cps->data[j] = tmp;
		}

		qsort(cps->data, n_samples, sizeof *cps->data, compare_cp);
	}

	count = fwrite(cps->data, sizeof *cps->data, n_samples, f);
	if (count != n_samples) {
		if (ferror(f))
			perror(pathbuf);
		else
			fprintf(stderr, "%s: end of file encountered while writing\n", pathbuf);

		fclose(f);
		remove(pathbuf);
		return;
	}

	fclose(f);
}

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-l limit] [-f filename] [-n n_samples] [-s seed]\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	struct cp_slice old_cps, new_cps;
	struct compact_puzzle cp;
	int optchar, i, limit = INT_MAX;
	size_t n_samples = 1000000;
	const char *samplefile = NULL;

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

	if (argc != optind)
		usage(argv[0]);

	cps_init(&new_cps);
	pack_puzzle(&cp, &solved_puzzle);
	cps_append(&new_cps, &cp);
	do_sampling(samplefile, &new_cps, 0, n_samples);

	for (i = 1; i <= limit; i++) {
		printf("%zu\n", new_cps.len);
		fflush(stdout);

		old_cps = new_cps;
		cps_init(&new_cps);

		do_round(&new_cps, &old_cps);
		if (samplefile != NULL)
			do_sampling(samplefile, &new_cps, i, n_samples);

		cps_free(&old_cps);
	}
}
