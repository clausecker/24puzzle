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

/* puzzle.c -- manipulating struct puzzle */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "puzzle.h"
#include "tileset.h"

/*
 * A solved puzzle configuration.
 */
const struct puzzle solved_puzzle = {
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
	{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
};

extern int
puzzle_parity(const struct puzzle *p)
{
	tileset ts = FULL_TILESET;
	int i, start, len, parity = zero_location(p);

	/* count cycle lengths */
	while (!tileset_empty(ts)) {
		len = 0;
		start = i = tileset_get_least(ts);
		do {
			ts = tileset_remove(ts, i);
			i = p->grid[i];
			len++;
		} while (i != start);
		parity ^= len ^ 1;
	}

	return (parity & 1);
}

/*
 * Describe p as a string and write the result to str.  The format is
 * equal to the format parsed by puzzle_parse().
 */
extern void
puzzle_string(char str[PUZZLE_STR_LEN], const struct puzzle *p)
{
	size_t i;

	for (i = 0; i < TILE_COUNT; i++)
		str += sprintf(str, "%d,", p->grid[i]);

	str[-1] = '\0';
}

/*
 * Print the board represented by p and write the result to str.
 */
extern void
puzzle_visualization(char str[PUZZLE_STR_LEN], const struct puzzle *p)
{
	size_t i;

	for (i = 0; i < TILE_COUNT; i++)
		if (p->grid[i] == 0)
			str += sprintf(str, "  %c", i % 5 == 4 ? '\n' : ' ');
		else
			str += sprintf(str, "%2d%c", (int)p->grid[i], i % 5 == 4 ? '\n' : ' ');
}

/*
 * Parse a puzzle configuration from str and store it in p.  Return 0
 * if parsing was succesful, -1 otherwise.  In case of failure, *p is
 * undefined.
 */
extern int
puzzle_parse(struct puzzle *p, const char *str)
{
	size_t i;
	long component;

	memset(p, 0, sizeof *p);
	memset(p->tiles, 0xff, TILE_COUNT);

	for (i = 0; i < TILE_COUNT; i++) {
		component = strtol(str, (char **)&str, 10);
		if (component < 0 || component >= TILE_COUNT)
			return (-1);

		if (p->tiles[component] != 0xff)
			return (-1);

		while (isspace(*str))
			str++;

		if (*str != ',' && i < TILE_COUNT - 1)
			return (-1);

		p->grid[i] = component;
		p->tiles[component] = i;

		str++;
	}

	return (0);
}
