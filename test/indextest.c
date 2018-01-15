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

/* indextest.c -- test if the various index functions work correctly */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "puzzle.h"
#include "tileset.h"
#include "index.h"
#include "random.h"

#define TEST_TS 0x00000fe

/*
 * Check if idx1 and idx2 refer to the same index with respect to ts.
 * Return 1 if they do, 0 if they do not.
 */
static int
index_equal(tileset ts, struct index *idx1, const struct index *idx2)
{
	if (idx1->pidx != idx2->pidx || idx1->maprank != idx2->maprank)
		return (0);

	if (tileset_has(ts, ZERO_TILE) && idx1->eqidx != idx2->eqidx)
		return (0);

	return (1);
}

/*
 * Round-trip p through compute_index() and check if we get the same
 * puzzle back.  Return 1 if we do, return 0 and print some information
 * if we don't.
 */
static int
test_puzzle(const struct index_aux *aux, const struct puzzle *p)
{
	char puzzle_str[PUZZLE_STR_LEN], index_str[INDEX_STR_LEN];
	struct puzzle pp;
	struct index idx;

	compute_index(aux, &idx, p);
	invert_index(aux, &pp, &idx);

	if (!puzzle_partially_equal(p, &pp, aux)) {
		printf("test_puzzle failed for 0x%07x:\n", aux->ts);
		puzzle_string(puzzle_str, p);
		puts(puzzle_str);
		index_string(aux->ts, index_str, &idx);
		puts(index_str);
		puzzle_string(puzzle_str, &pp);
		puts(puzzle_str);

		return (0);
	}

	return (1);
}

/*
 * Round-trip idx through inverse_index() and check if we get the same
 * index back.  Return 1 if we do, return 0 and print some information
 * if we don't.
 */
static int
test_index(const struct index_aux *aux, const struct index *idx)
{
	char puzzle_str[PUZZLE_STR_LEN], index_str[INDEX_STR_LEN];
	struct index idx2;
	struct puzzle p;

	invert_index(aux, &p, idx);
	compute_index(aux, &idx2, &p);

	if (!index_equal(aux->ts, &idx2, idx)) {
		printf("test_index failed for 0x%07x:\n", aux->ts);
		index_string(aux->ts, index_str, idx);
		puts(index_str);
		puzzle_string(puzzle_str, &p);
		puts(puzzle_str);
		index_string(aux->ts, index_str, &idx2);
		puts(index_str);

		return (0);
	}

	return (1);
}

static void
usage(char *argv0)
{
	printf("Usage: %s [-i iterations] [-t tile,tile,...]\n", argv0);
}

extern int
main(int argc, char *argv[])
{
	size_t i, n = 10000;
	struct puzzle p;
	struct index idx;
	struct index_aux aux;
	tileset ts = TEST_TS;
	int optchar;

	while (optchar = getopt(argc, argv, "i:t:"), optchar != -1)
		switch (optchar) {
		case 'i':
			n = atol(optarg);
			break;

		case 't':
			if (tileset_parse(&ts, optarg) != 0) {
				printf("Invald tileset: %s\n", optarg);
				usage(argv[0]);
			}

			break;

		default:
			usage(argv[0]);
		}

	random_seed = time(NULL);
	make_index_aux(&aux, ts);

	for (i = 0; i < n; i++) {
		random_puzzle(&p);
		if (!test_puzzle(&aux, &p))
			return (EXIT_FAILURE);
	}

	for (i = 0; i < n; i++) {
		random_index(&aux, &idx);
		if (!test_index(&aux, &idx))
			return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}
