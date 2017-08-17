#ifndef PDB_H
#define PDB_H

#include <stdio.h>
#include <limits.h>

#include "tileset.h"
#include "index.h"

/*
 * A patterndb is an array containing for each partial puzzle
 * configuration (called pattern) the minimal number of moves needed
 * to transition the pattern into a solved configuration.
 */
typedef _Atomic unsigned char *patterndb;

/*
 * A value representing an infinite distance to the solved position,
 * i.e. a PDB entry that hasn't been filled in yet.
 */
enum { INFINITY = (unsigned char)-1 };

enum {
	/* max number of jobs allowed */
	PDB_MAX_JOBS = 256,

	/* maximum number of entries in a PDB histogram */
	PDB_HISTOGRAM_LEN = 256,
};

/*
 * The number of threads to use.  This must be between 1 and
 * PDB_MAX_JOBS and is set to 1 initially.  This is a global variable
 * intended to be set once during program initialization.  Since its
 * value typically does not change during operation, the author deemed
 * it more useful to have this be a global variable instead of passing
 * it around everywhere.
 */
extern int pdb_jobs;

extern int	generate_patterndb(patterndb, tileset, FILE *);
extern int	verify_patterndb(patterndb, tileset, FILE *);
extern int	generate_pdb_histogram(cmbindex[PDB_HISTOGRAM_LEN], patterndb, tileset);
extern void	reduce_patterndb(patterndb, tileset, FILE *);

#endif /* PDB_H */
