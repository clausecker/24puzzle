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

	memset(histogram, 0, sizeof histogram);

	for (i = 0; i < n; i++)
		histogram[cfg->pcfg.pdb->tables[idx->maprank][i]]++;

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
