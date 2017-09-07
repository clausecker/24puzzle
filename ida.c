/* ida.c -- the IDA* algorithm */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "search.h"
#include "pdb.h"
#include "puzzle.h"
#include "tileset.h"

/*
 * The search path as we store it while searching.  This very compressed
 * layout contains the following parts:
 *
 * - zloc is the location of the zero tile
 * - nextmove is the next move to try
 * - oldh and oldht are the old values of hparts[] and htparts[] for the
 *   entry that changed.
 *
 * We only store oldh and oldht since only one summand of h changes
 * when moving a tile.  This is because each PDB considers a disjoint
 * tileset and as move disturbs one tile, only one tileset's partial
 * configuration is changed.
 *
 * The path begins with two dummy nodes to simplify the code that
 * excludes useless moves.
 */
struct search_node {
	unsigned char zloc, nextmove, oldh, oldht;
};

/*
 * Push a node onto the search path, update p and pt and compute heuristics
 * for the new node.
 */
static void
path_push(struct search_node *path, int *dist, struct patterndb **pdbs, size_t n_pdb,
    struct puzzle *p, unsigned char *hparts, unsigned *h,
    struct puzzle *pt, unsigned char *htparts, unsigned *ht,
    unsigned dest)
{
	/* the tiles we move */
	size_t tile = p->grid[dest], ttile = pt->grid[transpositions[dest]];
	size_t i;

	assert(tile != 0);
	assert(ttile != 0);

	move(p, dest);
	move(pt, transpositions[dest]);

	++*dist;
	path[*dist].zloc = zero_location(p);
	path[*dist].nextmove = 0;

	/* find out which PDB's tileset we change and update h values */
	for (i = 0; i < n_pdb; i++) {
		if (!tileset_has(pdbs[i]->aux.ts, tile))
			continue;

		path[*dist].oldh = hparts[i];
		*h -= hparts[i];
		hparts[i] = pdb_lookup_puzzle(pdbs[i], p);
		*h += hparts[i];

		break;
	}

	assert(i != n_pdb);

	for (i = 0; i < n_pdb; i++) {
		if (!tileset_has(pdbs[i]->aux.ts, ttile))
			continue;

		path[*dist].oldht = htparts[i];
		*ht -= htparts[i];
		htparts[i] = pdb_lookup_puzzle(pdbs[i], pt);
		*ht += htparts[i];

		break;
	}

	assert (i != n_pdb);
}

/*
 * Pop a node off the stack and restore the previous p, pt, h, ht,
 * hparts, and htparts.
 */
static void
path_pop(struct search_node *path, int *dist, struct patterndb **pdbs, size_t n_pdb,
    struct puzzle *p, unsigned char *hparts, unsigned *h,
    struct puzzle *pt, unsigned char *htparts, unsigned *ht)
{
	size_t tile, ttile;
	size_t i;

	--*dist;
	tile = p->grid[path[*dist].zloc];
	ttile = pt->grid[transpositions[path[*dist].zloc]];

	move(p, path[*dist].zloc);
	move(pt, transpositions[path[*dist].zloc]);

	/* find out which PDB's tileset we need to restore the h values from */
	for (i = 0; i < n_pdb; i++) {
		if (!tileset_has(pdbs[i]->aux.ts, tile))
			continue;

		*h -= hparts[i];
		hparts[i] = path[*dist + 1].oldh;
		*h += hparts[i];

		break;
	}

	assert (i != n_pdb);

	for (i = 0; i < n_pdb; i++) {
		if (!tileset_has(pdbs[i]->aux.ts, ttile))
			continue;

		*ht -= htparts[i];
		htparts[i] = path[*dist + 1].oldht;
		*ht += htparts[i];

		break;
	}

	assert (i != n_pdb);
}

/*
 * Search for a solution for p through the search space.  Do not exceed
 * a total distance of *bound.  Store the number of nodes expanded in
 * *expanded.  Return 0 if a solution was found, -1 if not.  Print
 * diagnostic messages to f if f is not NULL.
 */
static int
search_to_bound(struct patterndb **pdbs, size_t n_pdb, const struct puzzle *parg,
    struct search_node path[SEARCH_PATH_LEN + 2], size_t *bound, FILE *f, unsigned long long *expanded)
{
	struct puzzle p = *parg;
	struct puzzle pt = p;
	size_t i, newbound = -1;
	unsigned char hparts[PDB_MAX_COUNT], htparts[PDB_MAX_COUNT];
	unsigned h, ht, hmax, dest;
	int dist;

	transpose(&pt);

	/* initialize the dummy nodes */
	path[0].zloc = -1; /* dummy value */
	path[0].nextmove = -1; /* dummy value */
	path[0].oldh = UNREACHED; /* dummy value */
	path[0].oldht = UNREACHED; /* dummy value */

	path[1].zloc = zero_location(&p);
	path[1].nextmove = 0;
	path[1].oldh = UNREACHED; /* dummy value */
	path[1].oldht = UNREACHED; /* dummy value */

	/*
	 * for easier programming, we want path[0] to be the root node,
	 * not the first dummy node, so we make path point right at it.
	 */
	path += 1;
	dist = 0;

	/* initialize predictors */
	h = ht = 0;
	for (i = 0; i < n_pdb; i++) {
		hparts[i] = pdb_lookup_puzzle(pdbs[i], &p);
		h += hparts[i];
		htparts[i] = pdb_lookup_puzzle(pdbs[i], &pt);
		ht += htparts[i];
	}

	/* do graph search bounded by bound */
	do {
		/* are we over the bound or out of moves to make? */
		hmax = h > ht ? h : ht;
		if (hmax + dist > *bound ||
		    path[dist].nextmove >= move_count(zero_location(&p))) {
			if (hmax + dist > *bound && hmax + dist < newbound)
				newbound = hmax + dist;

			path_pop(path, &dist, pdbs, n_pdb, &p, hparts, &h, &pt, htparts, &ht);
		} else { /* make the next move */
			dest = get_moves(zero_location(&p))[path[dist].nextmove++];

			/* would we move back to the previous configuration? */
			if (dest == path[dist - 1].zloc)
				continue;

			path_push(path, &dist, pdbs, n_pdb, &p, hparts, &h, &pt, htparts, &ht, dest);
			++*expanded;

			/* have we found the solution? */
			if (h == 0 && ht == 0 && memcmp(p.tiles, solved_puzzle.tiles, TILE_COUNT) == 0) {
				if (f != NULL)
					fprintf(f, "Solution found at depth %d.\n", dist);

				*bound = dist;
				return (0);
			}
		}
	} while (dist >= 0); /* abort if nothing found */

	if (f != NULL)
		fprintf(f, "No solution found with bound %zu, increasing bound to %zu.\n",
		    *bound, newbound);

	assert(newbound != -1);
	*bound = newbound;

	/* nothing found */
	return (-1);
}

/*
 * Try to find a solution for parg wit the IDA* algorithm using the
 * disjoint pattern databases pdbs as heuristic functions.  Store the
 * path found in path and return the number of nodes in the path.  If
 * f is not NULL, print diagnostic messages to f.
 */
extern size_t
search_ida(struct patterndb **pdbs, size_t n_pdb,
    const struct puzzle *p, struct path *path, FILE *f)
{
	struct search_node spath[SEARCH_PATH_LEN + 2];
	unsigned long long expanded, total_expanded = 0;
	size_t i, bound = 0;
	int unfinished;

	do {
		expanded = 0;
		unfinished = search_to_bound(pdbs, n_pdb, p, spath, &bound, f, &expanded);
		total_expanded += expanded;

		if (f != NULL)
			fprintf(f, "Expanded %llu nodes during previous round.\n", expanded);
	} while (unfinished);

	if (f != NULL)
		fprintf(f, "Expanded %llu nodes in total.\n", total_expanded);

	/* copy spath to path */
	path->pathlen = bound - 1;
	for (i = 1; i < bound; i++)
		path->moves[i] = spath[i + 2].zloc;

	return (bound);
}
