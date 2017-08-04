/* indextest.c -- test if the various index functions work correctly */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "puzzle.h"
#include "tileset.h"
#include "index.h"

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
 * Round-trip p through compute_index() and check if we get the same
 * puzzle back.  Return 1 if we do, return 0 and print some information
 * if we don't.
 */
static int
test_puzzle(const struct puzzle *p)
{
	char puzzle_str[PUZZLE_STR_LEN], index_str[INDEX_STR_LEN];
	struct puzzle pp;
	struct index idx;

	compute_index(FULL_TILESET, &idx, p);
	invert_index(FULL_TILESET, &pp, &idx);

	if (memcmp(p, &pp, sizeof pp) != 0) {
		printf("test_puzzle failed:\n");
		puzzle_string(puzzle_str, p);
		puts(puzzle_str);
		index_string(FULL_TILESET, index_str, &idx);
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
test_index(const struct index *idx)
{
	char puzzle_str[PUZZLE_STR_LEN], index_str[INDEX_STR_LEN];
	struct index idx2;
	struct puzzle p;

	invert_index(FULL_TILESET, &p, idx);
	compute_index(FULL_TILESET, &idx2, &p);

	if (memcmp(&idx2, idx, sizeof idx2) != 0) {
		printf("test_index failed:\n");
		index_string(FULL_TILESET, index_str, idx);
		puts(index_str);
		puzzle_string(puzzle_str, &p);
		puts(puzzle_str);
		index_string(FULL_TILESET, index_str, &idx2);
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
		if (!test_puzzle(&p))
			return (EXIT_FAILURE);
	}

	for (i = 0; i < n; i++) {
		random_index(&idx);
		if (!test_index(&idx))
			return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}
