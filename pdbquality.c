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

/*
 * Configuration for pdb_quality.  quality is the accumulator for the
 * pattern database quality.
 */
struct pdbquality_config {
	struct parallel_config pcfg;
	atomic_llong quality;
};

static void
quality_worker(void *cfgarg, struct index *idx)
{
	struct pdbquality_config *cfg = cfgarg;
	struct patterndb *pdb = cfg->pcfg.pdb;
	long long int quality = 0, accum;
	size_t n_eqclass = eqclass_count(&pdb->aux, idx->maprank);
	tileset zreg;

	for (idx->eqidx = 0; idx->eqidx < n_eqclass; idx->eqidx++) {
		accum = 0;
		for (idx->pidx = 0; idx->pidx < pdb->aux.n_perm; idx->pidx++)
			accum += pdb_lookup(pdb, idx);

		zreg = eqclass_from_index(&pdb->aux, idx);
		quality += accum * tileset_count(zreg);
	}

	cfg->quality += quality;
}

/*
 * Compute the quality of PDB.  The quality of a PDB is the sum of all h
 * values in the PDB weighted by the size of the zero tile region.  This
 * number is proportional to the average h value and can be used to find
 * good pattern databases.
 */
extern long long int
pdb_quality(struct patterndb *pdb)
{
	struct pdbquality_config cfg;

	cfg.pcfg.pdb = pdb;
	cfg.pcfg.worker = quality_worker;
	cfg.quality = 0;

	pdb_iterate_parallel(&cfg.pcfg);

	return (cfg.quality);
}
