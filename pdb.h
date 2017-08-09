#ifndef PDB_H
#define PDB_H

#include <stdio.h>

#include "tileset.h"

/*
 * A patterndb is an array containing for each partial puzzle
 * configuration (called pattern) the minimal number of moves needed
 * to transition the pattern into a solved configuration.
 */
typedef unsigned char *patterndb;

extern int	generate_patterndb(patterndb, tileset, FILE *);

#endif /* PDB_H */
