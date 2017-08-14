/* tileset.c -- dealing with sets of tiles */

#include <stdlib.h>
#include <string.h>

#include "puzzle.h"
#include "tileset.h"

/*
 * Given a tileset cmap representing free positions on the grid and
 * a square number t set in cmap, return a tileset representing
 * all squares reachable from t through members of ts.
 */
static tileset
tileset_flood(tileset cmap, unsigned t)
{
	tileset r = tileset_add(EMPTY_TILESET, t), oldr;

	do {
		oldr = r;

		/*
		 * the mask prevents carry into other rows:
		 * 0x0f7bdef: 01111 01111 01111 01111 01111
		 */
		r = cmap & (r | r  << 5 | (r & 0x0f7bdef) << 1 | r >> 5 | r >> 1 & 0x0f7bdef);
	} while (oldr != r);

	return (r);
}

/*
 * For a tileset ts which may or may not contain the zero tile
 * and a puzzle configuration p, compute a tileset representing
 * the squares we can move the zero tile to without disturbing
 * the non-zero tiles in ts.  If the zero tile is not in ts, it
 * this is just the set of squares not occupied by tiles in ts
 * as we cannot make assumptions about the position of the zero
 * tile.
 */
extern tileset
tileset_eqclass(tileset ts, const struct puzzle *p)
{
	tileset cmap = tileset_complement(tileset_map(tileset_remove(ts, 0), p));

	if (tileset_has(ts, 0))
		return (tileset_flood(cmap, p->tiles[0]));
	else
		return (cmap);
}

/*
 * Generate a string representing ts and store it in str.
 */
extern void
tileset_string(char str[TILESET_STR_LEN], tileset ts)
{
	size_t i;

	strcpy(str, "         \n         \n         \n         \n         \n");

	for (i = 0; i < TILE_COUNT; i++)
		if (tileset_has(ts, i))
			str[2 * i] = 'X';
}

/*
 * Parse a tileset represented as a list of tiles.  Return 0 if a
 * tileset could be parsed succesfully, -1 otherwise.  On success,
 * *ts is the parsed tileset, otherwise, *ts is undefined.
 */
extern int
tileset_parse(tileset *ts, const char *str)
{
	long component;

	*ts = EMPTY_TILESET;

	for (;;) {
		component = strtol(str, (char**)&str, 10);
		if (component < 0 || component >= TILE_COUNT)
			return (-1);

		*ts = tileset_add(*ts, component);

		if (*str == '\0')
			break;

		if (*str != ',')
			return (-1);

		str++;
	}

	return (0);
}
