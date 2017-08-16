#ifndef PARALLEL_H
#define PARALLEL_H

#include "pdb.h"

/*
 * This file contains some helper functions for parallel iteration
 * through endgame tablebases.  The function pdb_iterate_parallel()
 * uses pdb_jobs threads to iterate through the PDB in chunks of
 * PDB_CHUNK_SIZE, calling a worker function for each chunk.  The
 * worker function can submit results by embedding
 * struct parallel_config at the beginning of a custom structure
 * like this:
 *
 *     struct my_config {
 *         struct parallel_config pcfg;
 *         _Atomic int widget_count;
 *         ...
 *     }
 */
struct parallel_config {
	_Atomic cmbindex offset;	/* start of next chunk to be done */
	cmbindex pdb_size;
	patterndb pdb;
	tileset ts;
	/* worker function */
	void (*worker)(void*, cmbindex, cmbindex);
};

enum {
	/* size of each chunk passed to the worker function */
	PDB_CHUNK_SIZE = 4096,
};

extern void pdb_iterate_parallel(struct parallel_config *);

#endif /* PARALLEL_H */
