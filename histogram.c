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

/* histogram.c -- generate PDB histograms */

#include <assert.h>
#include <string.h>

#include "index.h"
#include "pdb.h"
#include "parallel.h"

/*
 * Control structure for parallel histogram generation.
 */
struct histogram_config {
	struct parallel_config pcfg;
	_Atomic size_t histogram[PDB_HISTOGRAM_LEN];
};

/*
 * Generate the histogram for one map.
 */
static void
histogram_worker(void *cfgarg, struct index *idx)
{
	struct histogram_config *cfg = cfgarg;
	size_t i, n = pdb_table_size(cfg->pcfg.pdb, idx->maprank);
	size_t histogram[PDB_HISTOGRAM_LEN];
	atomic_uchar *table = pdb_entry_pointer(cfg->pcfg.pdb, idx);

	memset(histogram, 0, sizeof histogram);

	for (i = 0; i < n; i++)
		histogram[table[i]]++;

	for (i = 0; i < PDB_HISTOGRAM_LEN; i++)
		cfg->histogram[i] += histogram[i];
}

/*
 * Given a pattern database pdb, count how many entries with each
 * distance exist and store the results in histogram.  Use up to
 * jobs threads for parallel computation.  Return the number of
 * nonzero entries in histogram.
 */
extern int
pdb_histogram(size_t histogram[PDB_HISTOGRAM_LEN],
    struct patterndb *pdb)
{
	struct histogram_config cfg;
	int i;

	cfg.pcfg.pdb = pdb;
	cfg.pcfg.worker = histogram_worker;
	memset((void*)cfg.histogram, 0, sizeof cfg.histogram);

	pdb_iterate_parallel(&cfg.pcfg);
	memcpy(histogram, (void*)cfg.histogram, sizeof cfg.histogram);

	for (i = 0; histogram[i] != 0; i++)
		;

	assert(i < PDB_HISTOGRAM_LEN);

	return (i);
}
