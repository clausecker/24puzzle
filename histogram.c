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
	_Atomic cmbindex histogram[PDB_HISTOGRAM_LEN];
};

/*
 * Generate one chunk of the histogram.
 */
static void
histogram_worker(void *cfgarg, cmbindex i0, cmbindex n)
{
	struct histogram_config *cfg = cfgarg;
	cmbindex i, histogram[PDB_HISTOGRAM_LEN];

	memset(histogram, 0, sizeof histogram);

	for (i = i0; i < i0 + n; i++)
		histogram[cfg->pcfg.pdb[i]]++;

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
generate_pdb_histogram(cmbindex histogram[PDB_HISTOGRAM_LEN], patterndb pdb,
    tileset ts)
{
	struct histogram_config cfg;
	int i;

	cfg.pcfg.pdb = pdb;
	cfg.pcfg.ts = ts;
	cfg.pcfg.worker = histogram_worker;
	memset((void*)cfg.histogram, 0, sizeof cfg.histogram);

	pdb_iterate_parallel(&cfg.pcfg);
	memcpy(histogram, (void*)cfg.histogram, sizeof cfg.histogram);

	for (i = 0; histogram[i] != 0; i++)
		;

	assert(i < PDB_HISTOGRAM_LEN);

	return (i);
}
