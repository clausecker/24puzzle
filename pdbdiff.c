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
	atomic_uchar *entryp;

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
		for (idx->pidx = 0; idx->pidx < pdb->aux.n_perm; idx->pidx++) {
			entryp = pdb_entry_pointer(pdb, idx);

			if (*entryp != UNREACHED)
				*entryp -= min;
		}
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
