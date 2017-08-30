/* pdbdiff.c -- reduce entropy with differential encoding */

#include "tileset.h"
#include "index.h"
#include "pdb.h"
#include "parallel.h"

/*
 * Configuration for pdb_diffcode().  minimums is an array of
 * eqclass_total(&pdb->aux) entries containing the minimum of
 * each equivalence class.
 */
struct pdbdiff_config {
	struct parallel_config pcfg;
	unsigned char *minimums;
};

/*
 * Apply differential coding to one map.
 */
static void
diffcode_map(void *cfgarg, struct index *idx)
{
	struct pdbdiff_config *cfg = cfgarg;
	struct patterndb *pdb = cfg->pcfg.pdb;
	size_t n_eqclass, offset;
	unsigned char min, entry;

	n_eqclass = eqclass_count(&pdb->aux, idx->maprank);
	if (tileset_has(pdb->aux.ts, ZERO_TILE))
		offset = pdb->aux.idxt[idx->maprank].offset;
	else
		offset = idx->maprank;

	for (idx->eqidx = 0; idx->eqidx < n_eqclass; idx->eqidx++) {
		min = UNREACHED;

		/* find minimums */
		for (idx->pidx = 0; idx->pidx < pdb->aux.n_perm; idx->pidx++) {
			entry = pdb_lookup(pdb, idx);
			if (entry < min)
				min = entry;
		}

		cfg->minimums[offset + idx->eqidx] = min;

		/* apply differences */
		for (idx->pidx = 0; idx->pidx < pdb->aux.n_perm; idx->pidx++)
			*pdb_entry_pointer(pdb, idx) -= min;
	}
}

/*
 * Encode the PDB using a differential encoding: For each map, find the
 * lowest distance and then only store the difference between each entry
 * and the lowest distance.  Hopefully, this allows us to make the ANS
 * encoding more effective.  minimums must point to an array of
 * eqclass_total(&pdb->aux) entries.
 */
extern void
pdb_diffcode(struct patterndb *pdb, unsigned char minimums[])
{
	struct pdbdiff_config cfg;

	cfg.pcfg.pdb = pdb;
	cfg.pcfg.worker = diffcode_map;
	cfg.minimums = minimums;

	pdb_iterate_parallel(&cfg.pcfg);
}
