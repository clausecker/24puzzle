/* indextest.c -- test if the various index functions work correctly */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "puzzle.h"
#include "tileset.h"
#include "index.h"

#define TEST_TS 0xf0f0f

/*
 * Set p to a random puzzle configuration.  This function uses the
 * rand() random number generator to generate mediocre randomness.
 */
static void
random_puzzle(struct puzzle *p)
{
	size_t i, j;

	for (i = 0; i < TILE_COUNT; i++) {
		j = rand() % (i + 1);
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
random_index(struct index *idx)
{
	size_t i;

	for (i = 0; i < TILE_COUNT; i++)
		idx->cmp[i] = rand() % (TILE_COUNT - i);
}

/*
 * Check if p1 and p2 are the same configuration with respect to the
 * tiles in ts.  Return 1 if they are, 0 if they are not.
 */
static int
puzzle_equal(tileset ts, const struct puzzle *p1, const struct puzzle *p2)
{
	size_t i;

	for (; !tileset_empty(ts); ts = tileset_remove_least(ts)) {
		i = tileset_get_least(ts);
		if (p1->tiles[i] != p2->tiles[i])
			return (0);
	}

	return (1);
}

/*
 * Round-trip p through compute_index() and check if we get the same
 * puzzle back.  Return 1 if we do, return 0 and print some information
 * if we don't.
 */
static int
test_puzzle(tileset ts, const struct puzzle *p)
{
	char puzzle_str[PUZZLE_STR_LEN], index_str[INDEX_STR_LEN];
	struct puzzle pp;
	struct index idx;
	cmbindex cmbstep, cmbfull;

	compute_index(ts, &idx, p);
	invert_index(ts, &pp, &idx);

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

	cmbstep = combine_index(ts, &idx);
	cmbfull = full_index(ts, p);
	if (cmbstep != cmbfull) {
		printf("test_puzzle failed for 0x%07x:\n", ts);
		puzzle_string(puzzle_str, p);
		puts(puzzle_str);
		printf("%llu != %llu\n", cmbstep, cmbfull);

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
test_index(tileset ts, const struct index *idx)
{
	char puzzle_str[PUZZLE_STR_LEN], index_str[INDEX_STR_LEN];
	struct index idx2, idx3;
	struct puzzle p;
	cmbindex cmb;

	invert_index(ts, &p, idx);
	compute_index(ts, &idx2, &p);

	cmb = combine_index(ts, idx);
	split_index(ts, &idx3, cmb);

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

extern int
main(int argc, char *argv[])
{
	size_t i, n = 10000;
	struct puzzle p;
	struct index idx;

	if (argc == 2)
		n = atol(argv[1]);

	srand(time(NULL));

	for (i = 0; i < n; i++) {
		random_puzzle(&p);
		if (!test_puzzle(TEST_TS, &p))
			return (EXIT_FAILURE);
	}

	for (i = 0; i < n; i++) {
		random_index(&idx);
		if (!test_index(TEST_TS, &idx))
			return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}
