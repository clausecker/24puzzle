/* ida.c -- the IDA* algorithm */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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
 * - childno indicates what number child we are of our parent
 * - to_expand is a bitmap containg the nodes we need to expand from here.
 * - child_ph and child_pht stores our children's partial node evaluations
 * - child_h stores our childrens node evaluations.
 *
 * The path begins with two dummy nodes to simplify the code that
 * excludes useless moves.
 */
struct search_node {
	struct partial_hvals child_ph[4], child_pht[4];
	unsigned zloc, childno, to_expand, child_h[4];
};

/*
 * Fill in child_ph, child_pht, child_h, and to_expand in child.  Omit
 * the node that would go back from the expansion.
 */
static void
evaluate_expansions(struct search_node *path, struct puzzle *p, struct puzzle *pt,
    struct pdb_catalogue *cat)
{
	size_t i, dest, tile, ttile;
	const signed char *moves;
	unsigned h, ht;

	path->to_expand = 0;
	assert(path->zloc < TILE_COUNT);
	moves = get_moves(path->zloc);

	for (i = 0; i < 4; i++) {
		dest = moves[i];

		/* don't attempt to go back */
		if (dest == -1 || dest == path[-1].zloc)
			continue;

		path->to_expand |= 1 << i;

		tile = p->grid[dest];
		ttile = pt->grid[transpositions[dest]];

		move(p, dest);
		move(pt, transpositions[dest]);

		memcpy(path->child_ph + i, path[-1].child_ph + path->childno, sizeof path->child_ph[i]);
		memcpy(path->child_pht + i, path[-1].child_pht + path->childno, sizeof path->child_pht[i]);

		h = catalogue_diff_hvals(path->child_ph + i, cat, p, tile);
		ht = catalogue_diff_hvals(path->child_pht + i, cat, pt, ttile);

		move(p, path->zloc);
		move(pt, transpositions[path->zloc]);

		path->child_h[i] = h > ht ? h : ht;
	}
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
	size_t newbound = -1, dloc;
	unsigned h, ht, hmax, dest;
	int dist;

	transpose(&pt);

	/* initialize the dummy nodes */
	path[0].zloc = zero_location(&p); /* dummy value */
	path[0].childno = -1; /* dummy value */
	path[0].to_expand = 0; /* dummy value */
	h = catalogue_partial_hvals(&path[0].child_ph[0], cat, &p);
	ht = catalogue_partial_hvals(&path[0].child_pht[0], cat, &pt);
	path[0].child_h[0] = h > ht ? h : ht;

	/*
	 * for easier programming, we want path[0] to be the root node,
	 * not the first dummy node, so we make path point right at it.
	 */
	path += 1;
	dist = 0;

	path[0].zloc = zero_location(&p);
	path[0].childno = 0;
	evaluate_expansions(path, &p, &pt, cat);

	/* do graph search bounded by bound */
	do {
		hmax = path[dist - 1].child_h[path[dist].childno];

		/* are we out of moves to make? */
		if (path[dist].to_expand == 0) {
			dist--;
			move(&p, path[dist].zloc);
			move(&pt, transpositions[path[dist].zloc]);
		} else { /* make the next move */
			++*expanded;

			dest = ctz(path[dist].to_expand);
			hmax = path[dist].child_h[dest];
			path[dist].to_expand &= ~(1 << dest);
			dloc = get_moves(path[dist].zloc)[dest];
			dist++;

			/* would we go over the limit? */
			if (hmax + dist > *bound) {
				if (hmax + dist < newbound)
					newbound = hmax + dist;

				dist--;
				continue;
			}

			move(&p, dloc);
			move(&pt, transpositions[dloc]);

			path[dist].childno = dest;
			path[dist].zloc = dloc;
			evaluate_expansions(path + dist, &p, &pt, cat);

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
	struct search_node *spath;
	unsigned long long expanded, total_expanded = 0;
	size_t i, bound = 0;
	int unfinished;

	spath = malloc(sizeof *spath * (SEARCH_PATH_LEN + 2));
	if (spath == NULL) {
		perror("malloc");
		abort();
	}

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

	free(spath);

	return (total_expanded);
}
