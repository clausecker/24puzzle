#ifndef PDB_H
#define PDB_H

#include <stdio.h>

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
	/* max number of threads allowed */
	PDB_MAX_THREADS = 256,

	/* chunk size for parallel PDB processing */
	PDB_CHUNK_SIZE = 4096,
};

extern int	generate_patterndb(patterndb, tileset, int, FILE *);
extern int	verify_patterndb(patterndb, tileset, int, FILE *);
#endif /* PDB_H */
