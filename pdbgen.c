/* genpdb.c -- generate pattern databases */

#include <stdio.h>
#include <string.h>

#include "puzzle.h"
#include "tileset.h"
#include "index.h"
#include "pdb.h"

/*
 * A value representing an infinite distance to the solved position,
 * i.e. a PDB entry that hasn't been filled in yet.
 */
enum { INFINITY = (unsigned char)-1 };

/*
 * Initialize pdb for generating a new pattern dabase.  To do that,
 * each entry is initialized with INFINITY except for the equivalence
 * class of the starting position, which is initialized to 0.
 */
static cmbindex
setup_pdb(patterndb pdb, tileset ts)
{
	tileset eq = tileset_eqclass(ts, &solved_puzzle);
	int count = tileset_count(eq);
	struct puzzle p = solved_puzzle;
	struct index idx;

	memset(pdb, INFINITY, (size_t)search_space_size(ts));

	for (; !tileset_empty(eq); eq = tileset_remove_least(eq)) {
		move(&p, tileset_get_least(eq));
		compute_index(ts, &idx, &p);
		pdb[combine_index(ts, &idx)] = 0;
	}

	/*
	 * If ts contains the zero tile, each of the configurations in
	 * the equivalence class are distinct.  If it does not, they are
	 * all the same and we only added one configuration again and
	 * again.
	 */
	return (tileset_has(ts, 0) ? count : 1);
}

/*
 * Find all positions that are within one move from the equivalence
 * class of p and enter them into pdb with value round if the are not
 * yet reachable in the pdb.  Return the number of newly entered
 * positions.  This function essentially implements the inner loop of
 * generate_round(), see there for more pseudo-code.
 */
static cmbindex
update_pdb(patterndb pdb, tileset ts, struct puzzle *p, int round)
{
	struct puzzle newp;
	struct index idx;
	cmbindex cmb, count = 0;
	size_t sq, c, i;
	tileset eqclass = tileset_eqclass(ts, p);
	tileset req = tileset_reduce_eqclass(eqclass);
	const signed char *moves;

	for (; !tileset_empty(req); req = tileset_remove_least(req)) {
		sq = tileset_get_least(req);
		move(p, sq);

		c = move_count(sq);
		moves = get_moves(sq);
		for (i = 0; i < c; i++) {
			if (tileset_has(eqclass, moves[i]))
				continue;

			newp = *p;
			move(&newp, moves[i]);
			compute_index(ts, &idx, &newp);
			cmb = combine_index(ts, &idx);
			if (pdb[cmb] != INFINITY)
				continue;

			pdb[cmb] = round;
			count++;
		}
	}

	return (count);
}

/*
 * Perform one round of PDB generation where round > 0.  This function
 * returns the number of PDB entries updated.  The operation implemented
 * by this function can be described by the following pseudo
 * code:
 *
 * for i = 0 .. search_space_size(ts) - 1 {
 *     if (pdb[i] != round - 1) continue;
 *     p = invert_index(ts, i);
 *     ps = all_positions_in_equivalence_class(ts, p);
 *     dsts = all_positions_reachable_in_a_move(ps);
 *     rdsts = all_positions_not_in_eqclass(ps, dsts);
 *     for (all rdst in rdsts) {
 *         idx = generate_index(rdst);
 *         if (pdb[idx] == INFINITY) pdb[idx] = round;
 *     }
 * }
 *
 * The actual implementation is a bit "inside out" as we do not want to
 * generate the sets dsts and rdsts explicitly and try to save work as
 * much as possible.
 */
static cmbindex
generate_round(patterndb pdb, tileset ts, int round)
{
	struct puzzle p;
	struct index idx;
	cmbindex i, size = search_space_size(ts), count = 0;

	for (i = 0; i < size; i++) {
		if (pdb[i] != round - 1)
			continue;

		split_index(ts, &idx, i);
		invert_index(ts, &p, &idx);

		count += update_pdb(pdb, ts, &p, round);
	}

	return (count);
}

/*
 * Generate a PDB (pattern database) for tileset ts.  pdb must be
 * allocated by the caller to contain at least search_space_size(ts)
 * entries.  If f is not NULL, status updates are written to f after
 * each round.  This function returns the number of rounds needed to
 * fill the PDB. This number is one higher than the highest distance
 * encountered.
 */
extern int
generate_patterndb(patterndb pdb, tileset ts, FILE *f)
{
	cmbindex count;
	int round = 0;

	count = setup_pdb(pdb, ts);
	if (f != NULL)
		fprintf(f, "  0: %20llu\n", count);

	do {
		count = generate_round(pdb, ts, ++round);
		if (f != NULL)
			fprintf(f, "%3d: %20llu\n", round, count);
	} while (count != 0);

	return (round);
}
