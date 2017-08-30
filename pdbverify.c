/* pdbcheck.c -- validate a pattern database */

#include <stdio.h>
#include <stdlib.h>

#include "puzzle.h"
#include "tileset.h"
#include "index.h"
#include "pdb.h"
#include "parallel.h"

/*
 * Verify if p's entry pdist in a zero-aware pattern database pdb is
 * internally consistent with the remaining entries, checking the whole
 * equivalence class of p.  The following invariants must hold:
 *
 * 1. no entry has distance UNREACHED as each configuration can be solved
 * 2. each configuration directly reachable from p's equivalence class
 *    has a distance that differs by 1 from p's distance.
 * 3. there must be a configuration whose distance is exactly one lower
 *    than p's distance, i.e. progress must be possible
 *
 * If all invariants are fulfilled for all positions in the PDB, the
 * PDB is internally consistent.   In my paper, I show that this is both
 * necessary and sufficient for the PDB to be correct.  Furthermore, if
 * genpdb has been programmed correctly, it should only generate correct
 * PDBs.
 *
 * Return zero if the configuration is valid, nonzero if it is not.
 */
static int
verify_entry(struct patterndb *pdb, struct index *idx, FILE *f)
{
	struct puzzle p;
	struct index didx;
	struct move moves[MAX_MOVES];
	size_t i, n_move, zloc;
	int srcentry, progress = 0, dstentry;;
	char indexstr1[INDEX_STR_LEN], indexstr2[INDEX_STR_LEN];

	/* invariant 1 */
	srcentry = pdb_lookup(pdb, idx);
	if (srcentry == UNREACHED) {
		if (f != NULL) {
			index_string(pdb->aux.ts, indexstr1, idx);
			fprintf(f, "Entry has value UNREACHED: %s\n", indexstr1);
		}

		return (1);
	}

	invert_index(&pdb->aux, &p, idx);
	n_move = generate_moves(moves, eqclass_from_index(&pdb->aux, idx));
	zloc = zero_location(&p);

	for (i = 0; i < n_move; i++) {
		move(&p, moves[i].zloc);
		move(&p, moves[i].dest);

		compute_index(&pdb->aux, &didx, &p);
		dstentry = pdb_lookup(pdb, &didx);

		/* invariant 2 */
		if (abs(srcentry - dstentry) > 1) {
			if (f != NULL) {
				index_string(pdb->aux.ts, indexstr1, idx);
				index_string(pdb->aux.ts, indexstr2, &didx);

				fprintf(f, "%s -> %s with entry %d -> %d invalid\n",
				    indexstr1, indexstr2, srcentry, dstentry);
			}

			return (1);
		}

		if (dstentry < srcentry)
			progress = 1;

		move(&p, moves[i].zloc);
		move(&p, zloc);
	}

	/* invariant 3 */
	if (progress == 0 && srcentry != 0) {
		if (f != NULL) {
			index_string(pdb->aux.ts, indexstr1, idx);
			fprintf(f, "No progress possible from %s\n", indexstr1);
		}

		return (1);
	}

	return (0);
}

/*
 * Just as with genpdb, this structure controls one verification thread.
 * Each thread verifies the PDB in chunks of PDB_CHUNK_SIZE.
 */
struct verify_config {
	struct parallel_config pcfg;
	_Atomic int result;
	FILE *f;
};

/*
 * Verify one cohort of the PDB.  Set cfg->result to 1 if the PDB was
 * found to be inconsistent.
 */
static void
verify_cohort(void *cfgarg, struct index *idx)
{
	struct verify_config *cfg = cfgarg;
	struct patterndb *pdb = cfg->pcfg.pdb;
	size_t i, j, n_eqclass = eqclass_count(&pdb->aux, idx->maprank),
	    n_perm = pdb->aux.n_perm;
	int result = 0;

	for (i = 0; i < n_eqclass; i++) {
		idx->eqidx = i;
		for (j = 0; j < n_perm; j++) {
			idx->pidx = j;
			result |= verify_entry(pdb, idx, cfg->f);
		}
	}

	cfg->result |= result;
}

/*
 * Verify the pattern database pdb by checking if each entry is
 * consistent with the others.  If f is not NULL, inconsistencies are
 * printed to f.  For further details on the verification process, read
 * the comment above the function verify_position().  This function
 * returns zero if the pattern database was found to be consistent,
 * nonzero otherwise.
 */
extern int
pdb_verify(struct patterndb *pdb, FILE *f)
{
	struct verify_config cfg;

	cfg.pcfg.pdb = pdb;
	cfg.pcfg.worker = verify_cohort;
	cfg.result = 0;
	cfg.f = f;

	pdb_iterate_parallel(&cfg.pcfg);

	return (cfg.result);
}
