/* pdbident.c -- identify equivalence classes */

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "parallel.h"
#include "pdb.h"

/*
 * Identify equivalence classes in the table for idx->maprank.
 * Afterwards, reallocate the table to reduce memory usage.
 */
static void
identify_worker(void *cfgarg, struct index *idx)
{
	struct parallel_config *cfg = cfgarg;
	struct index_aux *aux = &cfg->pdb->aux;
	size_t pidx, eqidx;
	size_t n_eqclass = aux->idxt[idx->maprank].n_eqclass, n_perm = aux->n_perm;
	int min, entry;
	atomic_uchar *table = cfg->pdb->tables[idx->maprank], *new_table;

	if (n_eqclass == 1)
		return;

	for (pidx = 0; pidx < n_perm; pidx++) {
		min = UNREACHED;
		for (eqidx = 0; eqidx < n_eqclass; eqidx++) {
			entry = table[eqidx * n_perm + pidx];
			if (entry < min)
				min = entry;
		}

		table[pidx] = min;
	}

	new_table = realloc(cfg->pdb->tables[idx->maprank], n_perm);
	if (new_table != NULL)
		cfg->pdb->tables[idx->maprank] = new_table;
}

/*
 * Identify equivalence classes of a pattern database.  This function
 * merges all equivalence classes of a given map into one, keeping only
 * the entry with the lowest distance.  Effectively, this removes the
 * zero-awareness of a PDB but yields a better heuristic than just
 * generating such a PDB in the first place.  The PDB is modified
 * in-place.  The pdb must not be  memory-mapped.  This fucntion is a
 * no-op if pdb is zero-unaware.
 */
extern void
pdb_identify(struct patterndb *pdb)
{
	tileset ts;
	struct parallel_config pcfg;

	assert(pdb->mapped == 0);
	if (!tileset_has(pdb->aux.ts, ZERO_TILE))
		return;

	pcfg.pdb = pdb;
	pcfg.worker = identify_worker;

	pdb_iterate_parallel(&pcfg);
	ts = pdb->aux.ts;
	make_index_aux(&pdb->aux, tileset_remove(ts, ZERO_TILE));
}
