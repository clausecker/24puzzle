#ifndef PARALLEL_H
#define PARALLEL_H

#include "pdb.h"

/*
 * This file contains some helper functions for parallel iteration
 * through endgame tablebases.  The function pdb_iterate_parallel()
 * uses pdb_jobs threads to iterate through the PDB, calling the worker
 * function for each maprank.  The worker function can submit results
 * by embedding struct parallel_config at the beginning of a custom
 * structure like this:
 *
 *     struct my_config {
 *         struct parallel_config pcfg;
 *         _Atomic int widget_count;
 *         ...
 *     }
 */
struct parallel_config {
	struct patterndb *pdb;
	_Atomic tsrank nextrank;	/* start of next chunk to be done */

	/* worker function */
	void (*worker)(void *, struct index *);
};

extern void pdb_iterate_parallel(struct parallel_config *);

#endif /* PARALLEL_H */
