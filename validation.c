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
