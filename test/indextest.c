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

#define TEST_TS 0x00000fe

/*
 * Check if p1 and p2 are the same configuration with respect to the
 * tiles in ts.  Return 1 if they are, 0 if they are not.
 */
static int
puzzle_equal(tileset ts, const struct puzzle *p1, const struct puzzle *p2)
{
	tileset tsnz;
	size_t i;

	for (tsnz = tileset_remove(ts, ZERO_TILE); !tileset_empty(tsnz); tsnz = tileset_remove_least(tsnz)) {
		i = tileset_get_least(tsnz);
		if (p1->tiles[i] != p2->tiles[i])
			return (0);
	}

	return (1);
}

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

	if (!puzzle_equal(aux->ts, p, &pp)) {
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
 * index back.  Also round-trip a random tile set through
 * combine_index() and see if we get the same result back.  Return 1
 * if we do, return 0 and print some information
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
