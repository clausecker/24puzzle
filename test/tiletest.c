/* tiletest.c -- check if tileset_eqclass works correctly */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "puzzle.h"
#include "tileset.h"

/*
 * Set p to a random puzzle configuration.  This function uses the
 * rand() random number generator to generate mediocre randomness.
 */
static void
random_puzzle(struct puzzle *p)
{
	size_t i, j;

	memset(p, 0, sizeof *p);

	for (i = 0; i < TILE_COUNT; i++) {
		j = rand() % (i + 1);
		p->tiles[i] = p->tiles[j];
		p->tiles[j] = i;
	}

	for (i = 0; i < TILE_COUNT; i++)
		p->grid[p->tiles[i]] = i;
}

extern int
main()
{
	struct puzzle p;
	tileset ts;
	char tsstr[TILESET_STR_LEN], puzzlestr[PUZZLE_STR_LEN];

	srand(time(NULL));
	ts = rand() & FULL_TILESET | 1;
	random_puzzle(&p);

	puzzle_string(puzzlestr, &p);
	tileset_string(tsstr, ts);
	printf("puzzle:\n%s\ntile set:\n%s\n", puzzlestr, tsstr);

	tileset_string(tsstr, tileset_add(EMPTY_TILESET, p.tiles[0]));
	printf("empty spot:\n%s\n", tsstr);

	tileset_string(tsstr, tileset_map(ts, &p));
	printf("map:\n%s\n", tsstr);

	tileset_string(tsstr, tileset_eqclass(ts, &p));
	printf("equivalence class:\n%s\n", tsstr);

	return (EXIT_SUCCESS);
}
