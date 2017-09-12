/* ida.c -- the IDA* algorithm */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "catalogue.h"
#include "pdb.h"
#include "puzzle.h"
#include "search.h"
#include "tileset.h"

/*
 * The search path as we store it while searching.  This
 * layout contains the following parts:
 *
 * - zloc is the location of the zero tile
 * - nextmove is the next move to try
 * - ph stores our partial node evaluation.
 *
 * The path begins with two dummy nodes to simplify the code that
 * excludes useless moves.
 */
struct search_node {
	struct partial_hvals ph, pht;
	unsigned char zloc, nextmove;
};

/*
 * Push a node onto the search path, update p and pt and compute heuristics
 * for the new node.  Return the new node's maximal h value.
 */
static unsigned
path_push(struct search_node *path, int *dist, struct pdb_catalogue *cat,
    struct puzzle *p, struct puzzle *pt, unsigned dest)
{
	/* the tiles we move */
	size_t tile = p->grid[dest], ttile = pt->grid[transpositions[dest]];
	unsigned h, ht;

	assert(tile != 0);
	assert(ttile != 0);

	move(p, dest);
	move(pt, transpositions[dest]);

	++*dist;
	path[*dist].ph = path[*dist - 1].ph;
	path[*dist].pht = path[*dist - 1].pht;
	path[*dist].zloc = zero_location(p);
	path[*dist].nextmove = 0;

	h = catalogue_diff_hvals(&path[*dist].ph, cat, p, tile);
	ht = catalogue_diff_hvals(&path[*dist].pht, cat, pt, ttile);

	return (h > ht ? h : ht);
}

/*
 * Pop a node off the stack, restore p and pt and return the h value of
 * the top node.
 */
static unsigned
path_pop(struct search_node *path, int *dist, struct pdb_catalogue *cat,
    struct puzzle *p, struct puzzle *pt)
{
	unsigned h, ht;

	if (--*dist < 0)
		return (-1); /* dummy value */

	move(p, path[*dist].zloc);
	move(pt, transpositions[path[*dist].zloc]);

	h = catalogue_ph_hval(cat, &path[*dist].ph);
	ht = catalogue_ph_hval(cat, &path[*dist].pht);

	return (h > ht ? h : ht);
}

/*
 * Search for a solution for p through the search space.  Do not exceed
 * a total distance of *bound.  Store the number of nodes expanded in
 * *expanded.  Return 0 if a solution was found, -1 if not.  Print
 * diagnostic messages to f if f is not NULL.
 */
static int
search_to_bound(struct pdb_catalogue *cat, const struct puzzle *parg,
    struct search_node path[SEARCH_PATH_LEN + 2], size_t *bound, FILE *f, unsigned long long *expanded)
{
	struct puzzle p = *parg;
	struct puzzle pt = p;
	size_t newbound = -1;
	unsigned h, ht, hmax, dest;
	int dist;

	transpose(&pt);

	/* initialize the dummy nodes */
	path[0].zloc = -1; /* dummy value */
	path[0].nextmove = -1; /* dummy value */
	/* dummy values */
	memset(&path[0].ph, -1, sizeof path[0].ph);
	memset(&path[0].pht, -1, sizeof path[0].pht);

	path[1].zloc = zero_location(&p);
	path[1].nextmove = 0;
	h = catalogue_partial_hvals(&path[1].ph, cat, &p);
	ht = catalogue_partial_hvals(&path[1].pht, cat, &pt);
	hmax = h > ht ? h : ht;

	/*
	 * for easier programming, we want path[0] to be the root node,
	 * not the first dummy node, so we make path point right at it.
	 */
	path += 1;
	dist = 0;

	/* do graph search bounded by bound */
	do {
		/* are we over the bound or out of moves to make? */
		if (hmax + dist > *bound ||
		    path[dist].nextmove >= move_count(zero_location(&p))) {
			if (hmax + dist > *bound && hmax + dist < newbound)
				newbound = hmax + dist;

			hmax = path_pop(path, &dist, cat, &p, &pt);
		} else { /* make the next move */
			dest = get_moves(zero_location(&p))[path[dist].nextmove++];

			/* would we move back to the previous configuration? */
			if (dest == path[dist - 1].zloc)
				continue;

			hmax = path_push(path, &dist, cat, &p, &pt, dest);
			++*expanded;

			/* have we found the solution? */
			if (hmax == 0 && memcmp(p.tiles, solved_puzzle.tiles, TILE_COUNT) == 0) {
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
 * path found in path and return the number of nodes expanded.  If
 * f is not NULL, print diagnostic messages to f.
 */
extern unsigned long long
search_ida(struct pdb_catalogue *cat, const struct puzzle *p, struct path *path, FILE *f)
{
	struct search_node spath[SEARCH_PATH_LEN + 2];
	unsigned long long expanded, total_expanded = 0;
	size_t i, bound = 0;
	int unfinished;

	do {
		expanded = 0;
		unfinished = search_to_bound(cat, p, spath, &bound, f, &expanded);
		total_expanded += expanded;

		if (f != NULL)
			fprintf(f, "Expanded %llu nodes during previous round.\n", expanded);
	} while (unfinished);

	if (f != NULL)
		fprintf(f, "Expanded %llu nodes in total.\n", total_expanded);

	/* copy spath to path */
	path->pathlen = bound;
	for (i = 0; i < bound; i++)
		path->moves[i] = spath[i + 2].zloc;

	return (expanded);
}
