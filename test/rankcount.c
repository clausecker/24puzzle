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

/* rankcount.c -- count the number equivalence classes for each map rank */

#include <stdlib.h>
#include <stdio.h>

#include "tileset.h"
#include "index.h"

extern int
main(int argc, char *argv[])
{
	struct index_aux aux;
	size_t max_eqclass, i;
	int tilecount;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s tilecount\n", argv[0]);
		return (EXIT_FAILURE);
	}

	tilecount = atoi(argv[1]);
	if (tilecount < 0 || tilecount >= TILE_COUNT) {
		fprintf(stderr, "Invalid tile count %s\n", argv[1]);
		return (EXIT_FAILURE);
	}

	/* +1 for zero tile */
	make_index_aux(&aux, tileset_least(tilecount + 1));
	max_eqclass = 0;

	for (i = 0; i < aux.n_maprank; i++)
		if (eqclass_count(&aux, i) > max_eqclass)
			max_eqclass = eqclass_count(&aux, i);

	printf("%d %zu %zu %.2f %zu\n", tilecount, (size_t)aux.n_maprank * aux.n_perm,
	    search_space_size(&aux), (double)eqclass_total(&aux) / aux.n_maprank,
	    max_eqclass);

	return (EXIT_SUCCESS);
}
