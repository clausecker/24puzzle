/* puzzle.c -- manipulating struct puzzle */

#include <assert.h>
#include <stdio.h>

#include "puzzle.h"

/* generate code for inline functions */
extern inline void	move();
extern inline size_t	move_count();
extern inline const signed char *get_moves();
/*
 * A solved puzzle configuration.
 */
const struct puzzle solved_puzzle = {
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
};


/*
 * List of all possible moves for a given position of the empty square.
 * There are up to four moves from every square, if there are less, the
 * remainder is filled up with -1.
 */
const signed char movetab[25][4] = {
	 1,  5, -1, -1,
	 0,  2,  6, -1,
	 1,  3,  7, -1,
	 2,  4,  8, -1,
	 3,  9, -1, -1,

	 0,  6, 10, -1,
	 1,  5,  7, 11,
	 2,  6,  8, 12,
	 3,  7,  9, 13,
	 4,  8, 14, -1,

	 5, 11, 15, -1,
	 6, 10, 12, 16,
	 7, 11, 13, 17,
	 8, 12, 14, 18,
	 9, 13, 19, -1,

	10, 16, 20, -1,
	11, 15, 17, 21,
	12, 16, 18, 22,
	13, 17, 19, 23,
	14, 18, 24, -1,

	15, 21, -1, -1,
	16, 20, 22, -1,
	17, 21, 23, -1,
	18, 22, 24, -1,
	19, 23, -1, -1,
};

/*
 * Describe p as a string and write the result to str.
 */
extern void
puzzle_string(char str[PUZZLE_STR_LEN], const struct puzzle *p)
{
	size_t i;

	for (i = 0; i < TILE_COUNT; i++)
		sprintf(str + 3 * i, "%2d ", p->tiles[i]);

	for (i = 0; i < TILE_COUNT; i++)
		sprintf(str + 3 * TILE_COUNT + 3 * i, "%2d ", p->grid[i]);

	str[3 * TILE_COUNT - 1] = '\n';
	str[2 * 3 * TILE_COUNT - 1] = '\n';
}
