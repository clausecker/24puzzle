/* validation.c -- validate struct puzzle */

#include <string.h>
#include <stdint.h>

#include "puzzle.h"

static int	perm_valid(const unsigned char[25]);

/*
 * Make sure that p is a valid puzzle.  Return 1 if it is, 0 if it is
 * not.  The following invariants must be fulfilled:
 *
 *  - every value between 0 and 24 must appear exactly once in p.
 *  - p->tiles and p->grid must be inverse to each other.
 */
extern int
puzzle_valid(const struct puzzle *p)
{
	unsigned char perm1[25], perm2[25];
	size_t i;

	if (!perm_valid(p->tiles) || !perm_valid(p->grid))
		return (0);

	for (i = 0; i < 25; i++)
		if (p->grid[p->tiles[i]] != i)
			return (0);

	return (1);
}

/*
 * Check if perm is a valid permutation of { 0, ..., 24 }.
 */
static int
perm_valid(const unsigned char perm[25])
{
	size_t i;
	uint_least32_t items = 0;

	for (i = 0; i < 25; i++) {
		if (perm[i] >= 25)
			return (0);

		if (items & 1LU << perm[i])
			return (0);

		items |= 1LU << perm[i];
	}

	return (1);
}
