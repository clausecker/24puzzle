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

/* pdbcount.c -- count the number of truly distinct PDBs */

#include <stdlib.h>
#include <stdio.h>

#include "tileset.h"
#include "transposition.h"

/*
 * Count how many unique PDBs with n_tiles tiles exist.  If zero is 0,
 * do not account for the zero tile.  If zero is 1, do account for the
 * zero tile.
 */
static void
count_pdbs(int n_tiles, int zero)
{
	size_t i, n = combination_count[n_tiles], c = 0;
	tileset t = tileset_least(n_tiles), ts;

	for (i = 0; i < n; i++) {
		ts = t << 1 | zero;
		if (canonical_automorphism(ts) == 0)
			c++;

		t = next_combination(t);
	}

	printf("%s: %20zu / %20zu (%5.2f%%)\n", zero ? "ZPDB" : "APDB",
	    c, (size_t)combination_count[n_tiles],
	    (100.0 * c) / combination_count[n_tiles]);
}

extern int
main(int argc, char *argv[])
{
	int n_tiles = 6;

	switch (argc) {
	case 0:
	case 1:
		break;

	case 2:
		n_tiles = atoi(argv[1]);
		break;

	default:
		fprintf(stderr, "Usage: %s [n_tiles]\n", argv[0]);
	}

	count_pdbs(n_tiles, 0);
	count_pdbs(n_tiles, 1);

	return (EXIT_SUCCESS);
}
