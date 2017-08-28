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
 * Update PDB entry idx by finding all positions we can move to from the
 * equivalence class represented by idx that are marked as UNREACHED and
 * then setting them to round.  Return the number of entries updated.
 */
static cmbindex
update_pdb_entry(struct patterndb *pdb, const struct index *idx,
    const struct move *moves, size_t n_move, int round)
{
	struct puzzle p;
	struct index didx, didx_prev;
	size_t i, zloc;
	cmbindex count = 0;

	invert_index(&pdb->aux, &p, idx);
	zloc = zero_location(&p);

	/* process one entry in advance so we can prefetch */
	move(&p, moves[0].zloc);
	move(&p, moves[0].dest);

	compute_index(&pdb->aux, &didx, &p);
	pdb_prefetch(pdb, &didx);
	didx_prev = didx;

	move(&p, moves[0].zloc);
	move(&p, zloc);

	for (i = 1; i < n_move; i++) {
		move(&p, moves[i].zloc);
		move(&p, moves[i].dest);

		compute_index(&pdb->aux, &didx, &p);
		pdb_prefetch(pdb, &didx);
		count += pdb_conditional_update(pdb, &didx_prev, round);
		didx_prev = didx;

		move(&p, moves[i].zloc);
		move(&p, zloc);
	}

	count += pdb_conditional_update(pdb, &didx_prev, round);

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
 * Generate one cohort of one round of the PDB where round > 0.
 * It is safe to execute this in parallel on the same dataset.
 */
static void
generate_cohort(void *cfgarg, struct index *idx)
{
	struct move moves[MAX_MOVES];
	struct pdbgen_config *cfg = cfgarg;
	struct patterndb *pdb = cfg->pcfg.pdb;
	size_t eqidx, pidx, n_eqclass = eqclass_count(&pdb->aux, idx->maprank),
	    n_perm = pdb->aux.n_perm, n_move;
	int round = cfg->round;
	cmbindex count = 0;

	for (eqidx = 0; eqidx < n_eqclass; eqidx++) {
		idx->eqidx = eqidx;
		n_move = generate_moves(moves, eqclass_from_index(&pdb->aux, idx));

		for (pidx = 0; pidx < n_perm; pidx++) {
			idx->pidx = pidx;
			if (pdb_lookup(pdb, idx) != round - 1)
				continue;

			count += update_pdb_entry(pdb, idx, moves, n_move, round);
		}
	}

	cfg->count += count;
}

/*
 * Generate a pattern database.  pdb must be allocated by the caller,
 * its content is erased in the process.  If f is not NULL, status
 * updates are written to f after each round.  This function returns
 * the number of rounds needed to fill the PDB.  This number is one
 * higher than the highest distance encountered.  Up to jobs threads
 * are used to compute the PDB in parallel.
 */
extern int
pdb_generate(struct patterndb *pdb, FILE *f)
{
	struct pdbgen_config cfg;
	struct index idx;

	cfg.pcfg.pdb = pdb;
	cfg.pcfg.worker = generate_cohort;
	cfg.round = 0;

	pdb_clear(pdb);
	compute_index(&pdb->aux, &idx, &solved_puzzle);
	pdb_update(pdb, &idx, 0);

	if (f != NULL)
		fprintf(f, "%3d: %20llu\n", 0, 1llu);

	do {
		cfg.count = 0;
		cfg.round++;
		pdb_iterate_parallel(&cfg.pcfg);
		if (f != NULL)
			fprintf(f, "%3d: %20llu\n", cfg.round, cfg.count);
	} while (cfg.count != 0);

	return (cfg.round);
}
