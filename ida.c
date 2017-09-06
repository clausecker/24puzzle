/* ida.c -- the IDA* algorithm */

#include <stdio.h>

#include "search.h"
#include "pdb.h"
#include "puzzle.h"
#include "tileset.h"

/*
 * Try to find a solution for parg wit the IDA* algorithm using the
 * disjoint pattern databases pdbs as heuristic functions.  Store the
 * path found in path and return the number of nodes in the path.  If
 * f is not NULL, print diagnostic messages to f.
 */
extern size_t
search_ida(struct patterndb **pdbs, size_t n_pdb,
    const struct puzzle *parg, struct path *path, FILE *f)
{
	// TODO
}
