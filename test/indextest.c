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

static unsigned random_seed;

/*
 * Run the xorshift random number generator for one iteration on
 * random_seed and return a random number.
 */
static unsigned
xorshift(void)
{
	unsigned state = random_seed;

	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;

	random_seed = state;

	return (state);
}

/*
 * Set p to a random puzzle configuration.  This function uses the
 * rand() random number generator to generate mediocre randomness.
 */
static void
random_puzzle(struct puzzle *p)
{
	size_t i, j;

	for (i = 0; i < TILE_COUNT; i++) {
		j = xorshift() % (i + 1);
		p->tiles[i] = p->tiles[j];
		p->tiles[j] = i;
	}

	for (i = 0; i < TILE_COUNT; i++)
		p->grid[p->tiles[i]] = i;
}

/*
 * Set i to a random index.  This function also uses rand().
 */
static void
random_index(struct index *idx, tileset ts)
{
	tileset tsnz = tileset_remove(ts, ZERO_TILE);

	idx->pidx = xorshift() % factorials[tileset_count(tsnz)];
	idx->maprank = xorshift() % combination_count[tileset_count(tsnz)];
	idx->eqidx = 0; /* TODO */
}

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
index_equal(tileset ts, const struct index *idx1, const struct index *idx2)
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
test_puzzle(tileset ts, const struct index_table *idxt, const struct puzzle *p)
{
	char puzzle_str[PUZZLE_STR_LEN], index_str[INDEX_STR_LEN];
	struct puzzle pp;
	struct index idx, idxsplit;
	cmbindex cmb;

	compute_index(ts, idxt, &idx, p);
	invert_index(ts, idxt, &pp, &idx);

	if (!puzzle_equal(ts, p, &pp)) {
		printf("test_puzzle failed for 0x%07x:\n", ts);
		puzzle_string(puzzle_str, p);
		puts(puzzle_str);
		index_string(ts, index_str, &idx);
		puts(index_str);
		puzzle_string(puzzle_str, &pp);
		puts(puzzle_str);

		return (0);
	}

	cmb = combine_index(ts, idxt, &idx);
	split_index(ts, idxt, &idxsplit, cmb);
	if (!index_equal(ts, &idx, &idxsplit)) {
		printf("test_puzzle failed for 0x%07x:\n", ts);
		puzzle_string(puzzle_str, p);
		puts(puzzle_str);
		index_string(ts, index_str, &idx);
		printf("%s\n%llu\n", index_str, cmb);
		index_string(ts, index_str, &idxsplit);
		puts(index_str);

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
test_index(tileset ts, const struct index_table *idxt, const struct index *idx)
{
	char puzzle_str[PUZZLE_STR_LEN], index_str[INDEX_STR_LEN];
	struct index idx2, idx3;
	struct puzzle p;
	cmbindex cmb;

	invert_index(ts, idxt, &p, idx);
	compute_index(ts, idxt, &idx2, &p);

	cmb = combine_index(ts, idxt, idx);
	split_index(ts, idxt, &idx3, cmb);

	if (memcmp(&idx2, idx, tileset_count(ts)) != 0) {
		printf("test_index failed for 0x%07x:\n", ts);
		index_string(ts, index_str, idx);
		puts(index_str);
		puzzle_string(puzzle_str, &p);
		puts(puzzle_str);
		index_string(ts, index_str, &idx2);
		puts(index_str);

		return (0);
	}

	if (memcmp(&idx3, idx, tileset_count(ts)) != 0) {
		printf("test_index failed for 0x%07x:\n", ts);
		index_string(ts, index_str, idx);
		puts(index_str);
		printf("%llu\n\n", cmb);
		index_string(ts, index_str, &idx3);
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
	struct index_table *idxt;
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
	idxt = make_index_table(ts);

	for (i = 0; i < n; i++) {
		random_puzzle(&p);
		if (!test_puzzle(ts, idxt, &p))
			return (EXIT_FAILURE);
	}

	for (i = 0; i < n; i++) {
		random_index(&idx, ts);
		if (!test_index(ts, idxt, &idx))
			return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}
