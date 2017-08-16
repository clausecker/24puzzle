/* genpdb.c -- generate pattern databases */

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "puzzle.h"
#include "tileset.h"
#include "index.h"
#include "pdb.h"
#include "parallel.h"

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

	memset((void*)pdb, INFINITY, (size_t)search_space_size(ts));

	/*
	 * If ts contains the zero tile, each of the configurations in
	 * the equivalence class are distinct.  If it does not, they are
	 * all the same and we only added one configuration.
	 */
	if (tileset_has(ts, ZERO_TILE)) {
		eq = tileset_eqclass(ts, &solved_puzzle);
		count = tileset_count(eq);

		for (; !tileset_empty(eq); eq = tileset_remove_least(eq)) {
			move(&p, tileset_get_least(eq));
			pdb[full_index(ts, &p)] = 0;
		}

		return (count);
	} else {
		pdb[full_index(ts, &p)] = 0;

		return (1);
	}
}

/*
 * Atomically update the PDB entry i to round:  If pdb[i] was
 * INFINITY before, set it to round and return 1.  Otherwise, do
 * nothing and return 0.  This function is atomic.
 */
static cmbindex
update_pdb_entry(patterndb pdb, cmbindex i, int round)
{

	if (pdb[i] != INFINITY)
		return (0);

	/* atomic_exchange in case we race with another thread */
	return (atomic_exchange(pdb + i, round) == INFINITY);
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
	cmbindex count = 0;
	size_t zloc = zero_location(p);
	tileset eq = tileset_eqclass(ts, p), mask;

	/*
	 * All positions in an equivalence class have the same value.
	 * Hence, if one entry is already filled in, we can savely omit
	 * the others.  Likely, once we verified that one entry is not
	 * filled in, we can fill in the others without checking.  There
	 * is no data race because each thread attempts to fill in the
	 * least entry in an equivalence class first and only one thread
	 * gets a successful compare-and-swap.
	 */
	move(p, tileset_get_least(eq));
	count = update_pdb_entry(pdb, full_index(ts, p), round);
	move(p, zloc);
	if (count == 0)
		return (0);

	for (mask = tileset_remove_least(eq); !tileset_empty(mask); mask = tileset_remove_least(mask)) {
		move(p, tileset_get_least(mask));
		pdb[full_index(ts, p)] = round;
		move(p, zloc);
	}

	return (tileset_count(eq));
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
	size_t i, zloc = zero_location(p), nmove = move_count(zloc);
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
	cmbindex count = 0;
	size_t i, nmove, zloc = zero_location(p);
	const signed char *moves;

	nmove = move_count(zloc);
	moves = get_moves(zloc);

	for (i = 0; i < nmove; i++) {
		if (tileset_has(eq, moves[i]))
			continue;

		move(p, moves[i]);
		count += update_pdb_entry(pdb, full_index(ts, p), round);

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
	size_t zloc = zero_location(p);
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
 * Configuration for generate_patterndb.  count is the total number of
 * entries updated in this round.
 */
struct pdbgen_config {
	struct parallel_config pcfg;
	_Atomic cmbindex count;
	int round;
};

/*
 * Generate one chunk of one round of the PDB where round > 0.  The
 * chunk starts at index i0 and is n entries long.  The caller must
 * ensure that the PDB is not read out of bounds.  This function
 * returns the number of PDB entries updated.  It is safe to execute
 * in parallel on the same dataset.
 */
static void
generate_round_chunk(void *cfgarg, cmbindex i0, cmbindex n)
{
	struct pdbgen_config *cfg = cfgarg;
	patterndb pdb = cfg->pcfg.pdb;
	tileset ts = cfg->pcfg.ts;
	int round = cfg->round;

	struct puzzle p;
	struct index idx;
	cmbindex i, count = 0;

	for (i = i0; i < i0 + n; i++) {
		if (pdb[i] != round - 1)
			continue;

		split_index(ts, &idx, i);
		invert_index(ts, &p, &idx);

		if (tileset_has(ts, ZERO_TILE))
			count += update_zero_pdb(pdb, ts, &p, round);
		else
			count += update_nonzero_pdb(pdb, ts, &p, round);
	}

	cfg->count += count;
}

/*
 * Generate a PDB (pattern database) for tileset ts.  pdb must be
 * allocated by the caller to contain at least search_space_size(ts)
 * entries.  If f is not NULL, status updates are written to f after
 * each round.  This function returns the number of rounds needed to
 * fill the PDB.  This number is one higher than the highest distance
 * encountered.  Up to jobs threads are used to compute the PDB in
 * parallel.
 */
extern int
generate_patterndb(patterndb pdb, tileset ts, FILE *f)
{
	struct pdbgen_config cfg;
	cmbindex count0;

	cfg.pcfg.pdb = pdb;
	cfg.pcfg.ts = ts;
	cfg.pcfg.worker = generate_round_chunk;
	cfg.round = 0;

	count0 = setup_pdb(pdb, ts);
	if (f != NULL)
		fprintf(f, "  0: %20llu\n", count0);

	do {
		cfg.count = 0;
		cfg.round++;
		pdb_iterate_parallel(&cfg.pcfg);
		if (f != NULL)
			fprintf(f, "%3d: %20llu\n", cfg.round, cfg.count);
	} while (cfg.count != 0);

	return (cfg.round);
}
