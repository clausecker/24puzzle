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

/* pdbquality.c -- determine PDB quality */

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "puzzle.h"
#include "tileset.h"
#include "index.h"
#include "pdb.h"
#include "parallel.h"
#include "statistics.h"

/*
 * From a PDB histogram, compute the quality of the PDB.  This is the
 * sum of all h in the PDB, weighted by the size of the zero tile
 * region.  This number is proportional to the average h value and can
 * be used to find good pattern databases.
 */
extern size_t
pdb_quality(const size_t histogram[PDB_HISTOGRAM_LEN])
{
	long long int quality = 0;
	size_t i;

	for (i = 0; i < PDB_HISTOGRAM_LEN; i++)
		quality += i * histogram[i];

	return (quality);
}

/*
 * Compute the partial eta value corresponding to a given PDB histogram.
 */
extern double
pdb_partial_eta(const size_t histogram[PDB_HISTOGRAM_LEN])
{
	double eta = 0.0, invb = 1.0 / B;
	size_t i, sum = 0;

	for (i = 0; i < PDB_HISTOGRAM_LEN; i++)
		sum += histogram[i];

	for (i = 1; i <= PDB_HISTOGRAM_LEN; i++)
		eta = histogram[PDB_HISTOGRAM_LEN - i] + eta * invb;

	eta /= (double)sum;

	return (eta);
}

/*
 * Compute the bias associated with the given zero tile region.  This is
 * the bias the PDB entries for that zero tile region enter the
 * calculation of eta with.
 */
static double
region_bias(tileset ts)
{
	double bias = 0.0;

	for (; !tileset_empty(ts); ts = tileset_remove_least(ts))
		bias += equilibrium_bias[tileset_get_least(ts)];

	return (bias);
}

/*
 * Compute eta for a complete pattern database.  Works for both APDBs
 * and ZPDBs.
 */
extern double
pdb_eta(struct patterndb *pdb)
{
	const struct index_aux *aux = &pdb->aux;
	double eta = 0.0;
	struct index idx;

	idx.pidx = 0;

	for (idx.maprank = 0; idx.maprank < aux->n_maprank; idx.maprank++)
		for (idx.eqidx = 0; idx.eqidx < eqclass_count(aux, idx.maprank); idx.eqidx++) {
			size_t i, histogram[PDB_HISTOGRAM_LEN];
			double map_eta = 0.0;
			const unsigned char *table;

			memset(histogram, 0, sizeof histogram);
			table = (const unsigned char *)pdb_entry_pointer(pdb, &idx);

			for (i = 0; i < aux->n_perm; i++)
				histogram[table[i]]++;

			for (i = 0; i < PDB_HISTOGRAM_LEN; i++)
				map_eta = histogram[PDB_HISTOGRAM_LEN - i - 1] + map_eta / B;

			eta += map_eta * region_bias(eqclass_from_index(aux, &idx));
		}

	eta /= (double)aux->n_perm * (double)aux->n_tile * (double)aux->n_maprank;

	return (eta);
}
