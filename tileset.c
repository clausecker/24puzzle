/*-
 * Copyright (c) 2017 Robert Clausecker. All rights reserved.
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

/* tileset.c -- dealing with sets of tiles */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
 * Set up an array mapping grid positions to the number of the
 * equivalence classes they are in.  Occupied spots are marked as -1.
 * Return the number of equivalence classes found.
 */
extern unsigned
tileset_populate_eqclasses(signed char eqclasses[TILE_COUNT], tileset map)
{
	unsigned n_eqclass;
	tileset i, eq, cmap = tileset_complement(map);

	/* first, mark all occupied spots */
	for (i = map; !tileset_empty(i); i = tileset_remove_least(i))
		eqclasses[tileset_get_least(i)] = -1;

	for (n_eqclass = 0; !tileset_empty(cmap); n_eqclass++) {
		eq = tileset_flood(cmap, tileset_get_least(cmap));
		cmap = tileset_difference(cmap, eq);
		for (i = eq; !tileset_empty(i); i = tileset_remove_least(i))
			eqclasses[tileset_get_least(i)] = n_eqclass;
	}

	return (n_eqclass);
}

/*
 * Generate a string representing ts and store it in str.
 */
extern void
tileset_string(char str[TILESET_STR_LEN], tileset ts)
{
	size_t i;
	char buf[3];

	memset(str, ' ', TILESET_STR_LEN - 1);
	str[TILESET_STR_LEN - 1] = '\0';

	for (i = 0; i < TILE_COUNT; i++) {
		if (tileset_has(ts, i)) {
			sprintf(buf, "%2zu", i);
			memcpy(str + 3 * i, buf, 2);
		}

		if (i % 5 == 4)
			str[3 * i + 2] = '\n';
	}
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

		while (isspace(*str))
			str++;

		if (*str == '\0')
			break;

		if (*str != ',')
			return (-1);

		str++;
	}

	return (0);
}
