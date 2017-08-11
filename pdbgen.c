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
	cmbindex count;
	tileset eq;
	struct puzzle p = solved_puzzle;
	struct index idx;

	memset(pdb, INFINITY, (size_t)search_space_size(ts));

	/*
	 * If ts contains the zero tile, each of the configurations in
	 * the equivalence class are distinct.  If it does not, they are
	 * all the same and we only added one configuration.
	 */
	if (tileset_has(ts, 0)) {
		eq = tileset_eqclass(ts, &solved_puzzle);
		count = tileset_count(eq);

		for (; !tileset_empty(eq); eq = tileset_remove_least(eq)) {
			move(&p, tileset_get_least(eq));
			compute_index(ts, &idx, &p);
			pdb[combine_index(ts, &idx)] = 0;
		}

		return (count);
	} else {
		compute_index(ts, &idx, &p);
		pdb[combine_index(ts, &idx)] = 0;

		return (1);
	}
}

/*
 * Set all positions in the same equivalence class as p that are marked
 * as INFINITY in pdb to round.  Return the number of positions updated.
 * This function may alter p intermediately but restores its former
 * value on return.
 */
static cmbindex
update_zero_eqclass(patterndb pdb, tileset ts, struct puzzle *p, int round)
{
	struct index idx;
	cmbindex cmb, count = 0;
	size_t zloc = p->tiles[0];
	tileset eq;

	for (eq = tileset_eqclass(ts, p); !tileset_empty(eq); eq = tileset_remove_least(eq)) {
		move(p, tileset_get_least(eq));
		compute_index(ts, &idx, p);
		cmb = combine_index(ts, &idx);

		if (pdb[cmb] != INFINITY) {
			/*
			 * All positions in an equivalence class have
			 * the same value. Hence, if one entry is
			 * already filled in, we can savely ignore the
			 * others.
			 */
			move(p, zloc);
			return (count); /* count is always 0 here */
		}

		pdb[cmb] = round;
		count++;

		/* undo move(p, tileset_get_least(eq)) */
		move(p, zloc);
	}


	return (count);
}

/*
 * Update one configuration of a PDB that accounts for the zero tile.
 * This is done by computing all moves from p, checking which of these
 * lead to a different equivalence class and then updating the PDB for
 * the equivalence classes of these configurations.
 */
static cmbindex
update_zero_pdb(patterndb pdb, tileset ts, struct puzzle *p, int round)
{
	cmbindex count = 0;
	size_t i, zloc = p->tiles[0], nmove = move_count(zloc);
	tileset eq = tileset_eqclass(ts, p);
	const signed char *moves = get_moves(zloc);

	for (i = 0; i < nmove; i++) {
		/* if we don't leave the equivalence class, skip this move */
		if (tileset_has(eq, moves[i]))
			continue;

		move(p, moves[i]);
		count += update_zero_eqclass(pdb, ts, p, round);

		/* undo move(p, moves[i]) */
		move(p, zloc);
	}

	return (count);
}

/*
 * Update all equivalence classes we can reach from one concrete
 * position in a PDB that does not account for the zero tile.  Return
 * the number of equivalence classes updated. eq should be equal to
 * tileset_eqclass(ts, p) and is passed as an argument so we don't need
 * to compute it over and over again.  p is modified by this function
 * but restored to its original value on return.  This function returns
 */
static cmbindex
update_nonzero_moves(patterndb pdb, tileset ts, struct puzzle *p, int round,
    tileset eq)
{
	struct index idx;
	cmbindex cmb, count = 0;
	size_t i, nmove, zloc = p->tiles[0];
	const signed char *moves;

	nmove = move_count(zloc);
	moves = get_moves(zloc);

	for (i = 0; i < nmove; i++) {
		if (tileset_has(eq, moves[i]))
			continue;

		move(p, moves[i]);
		compute_index(ts, &idx, p);
		cmb = combine_index(ts, &idx);
		if (pdb[cmb] == INFINITY) {
			pdb[cmb] = round;
			count++;
		}

		/* undo move(p, moves[i]) */
		move(p, zloc);
	}

	return (count);
}

/*
 * Update one equivalence class in a PDB that does not account for the
 * zero tile.  This is done by checking each possible move in the
 * reduced equivalence class of p and then updating those that move to a
 * different equivalence class.  This function returns the number of
 * entries updated.
 */
static cmbindex
update_nonzero_pdb(patterndb pdb, tileset ts, struct puzzle *p, int round)
{
	cmbindex count = 0;
	size_t zloc = p->tiles[0];
	tileset eq = tileset_eqclass(ts, p), req;

	for (req = tileset_reduce_eqclass(eq); !tileset_empty(req); req = tileset_remove_least(req)) {
		move(p, tileset_get_least(req));
		count += update_nonzero_moves(pdb, ts, p, round, eq);

		/* undo move(p, tileset_get_least(req)) */
		move(p, zloc);
	}

	return (count);
}

/*
 * Perform one round of PDB generation where round > 0.  This function
 * returns the number of PDB entries updated. TODO describe how this
 * works.
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

		if (tileset_has(ts, 0))
			count += update_zero_pdb(pdb, ts, &p, round);
		else
			count += update_nonzero_pdb(pdb, ts, &p, round);
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
