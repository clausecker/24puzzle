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

/* highest number of threads allowed for generate_patterndb() */
enum { GENPDB_MAX_THREADS = 256 };

extern int	generate_patterndb(patterndb, tileset, int, FILE *);
extern cmbindex	verify_patterndb(patterndb, tileset, FILE *);
#endif /* PDB_H */
