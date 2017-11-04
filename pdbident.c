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

/* pdbident.c -- turn a zero-aware PDB into a zero-unaware PDB */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "tileset.h"
#include "index.h"
#include "pdb.h"
#include "parallel.h"

/*
 * Identify the equivalence classes of the cohort given by idx.
 */
static void
identify_worker(void *cfgarg, struct index *idx)
{
	struct parallel_config *cfg = cfgarg;
	struct index_aux *aux = &cfg->pdb->aux;
	size_t pidx, eqidx, n_perm = aux->n_perm;
	size_t n_eqclass = eqclass_count(aux, idx->maprank);
	int min, entry;
	atomic_uchar *table = pdb_entry_pointer(cfg->pdb, idx);

	if (n_eqclass == 1)
		return;

	for (pidx = 0; pidx < n_perm; pidx++) {
		min = table[pidx];
		for (eqidx = 1; eqidx < n_eqclass; eqidx++) {
			entry = table[eqidx * n_perm + pidx];
			min = entry < min ? entry : min;
		}

		table[pidx] = min;
	}
}

/*
 * Rearrange the tables in pdb->data for the zero-unaware PDB we want.
 * Then, update the remaining fields as needed.
 */
static void
move_tables(struct patterndb *pdb)
{
	struct index idx;
	size_t n_maprank = pdb->aux.n_maprank, n_perm = pdb->aux.n_perm;
	void *oldloc, *newloc, *newdata;

	idx.pidx = 0;
	idx.eqidx = 0;
	for (idx.maprank = 0; idx.maprank < n_maprank; idx.maprank++) {
		oldloc = pdb_entry_pointer(pdb, &idx);
		newloc = pdb->data + idx.maprank * n_perm;
		if (oldloc != newloc)
			memcpy(newloc, oldloc, n_perm);
	}

	make_index_aux(&pdb->aux, tileset_remove(pdb->aux.ts, ZERO_TILE));
	newdata = realloc(pdb->data, search_space_size(&pdb->aux));
	if (newdata != NULL)
		pdb->data = newdata;
}

/*
 * Turn zero-aware PDB pdb into a zero-unaware PDB by identifying its
 * equivalence classes, choosing the minimum of all equivalence classes
 * distance for each map/perm.  If pdb is zero-unaware, this is a no-op.
 * Otherwise, it is assumed that pdb has been allocated, the function
 * overwrites the content of pdb with the new values in place.
 */
extern void
pdb_identify(struct patterndb *pdb)
{
	struct parallel_config cfg;

	if (!tileset_has(pdb->aux.ts, ZERO_TILE))
		return;

	assert(!pdb->mapped);

	cfg.pdb = pdb;
	cfg.worker = identify_worker;

	pdb_iterate_parallel(&cfg);
	move_tables(pdb);
}
