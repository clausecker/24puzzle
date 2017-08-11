#ifndef PDB_H
#define PDB_H

#include <stdio.h>

#include "tileset.h"

/*
 * A patterndb is an array containing for each partial puzzle
 * configuration (called pattern) the minimal number of moves needed
 * to transition the pattern into a solved configuration.
 */
typedef _Atomic unsigned char *patterndb;

enum { GENPDB_MAX_THREADS = 256 };

extern int	generate_patterndb(patterndb, tileset, int, FILE *);

#endif /* PDB_H */
