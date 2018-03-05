/*-
 * Copyright (c) 2018 Robert Clausecker. All rights reserved.
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

/* etacount.c -- Compute eta by counting */

#include <stdio.h>
#include <string.h>

#include "pdb.h"
#include "puzzle.h"
#include "parallel.h"
#include "index.h"

/*
 * This function computes the partial eta value for each cohort.  The
 * search space size is set such that each cohort must be added as often
 * as the size of the corresponding zero tile region to get the eta
 * value of the whole pattern database.
 */
double *
make_cohort_etas(struct patterndb *pdb)
{
	size_t i, j, n_tables = eqclass_total(&pdb->aux);
	size_t histogram[PDB_HISTOGRAM_LEN];
	double eta, *etas, scale;
	const atomic_uchar *table;

	etas = malloc(n_tables * sizeof *etas);
	if (etas == NULL)
		return (NULL);

	scale = 1.0 / ((double)pdb->aux.n_perm * pdb->aux.n_maprank *
	    (TILE_COUNT - pdb->aux.n_tile));

	for (i = 0; i < n_tables; i++) {
		memset(histogram, 0, sizeof histogram);
		table = pdb->data + i * pdb->aux.n_perm;
		for (j = 0; j < pdb->aux.n_perm; j++)
			histogram[table[j]]++;

		eta = 0.0;
		for (j = 1; j <= PDB_HISTOGRAM_LEN; j++)
			eta = histogram[PDB_HISTOGRAM_LEN - j] + B * eta;

		etas[i] = eta * scale;
	}

	return (etas);
}

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-f prefix] [-d pdbdir] [-j nproc] tileset tileset tileset tileset\n", argv0);
	exit(EXIT_FAILURE);
}
