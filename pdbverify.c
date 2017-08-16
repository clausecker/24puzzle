/* pdbcheck.c -- validate a pattern database */

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include "puzzle.h"
#include "tileset.h"
#include "index.h"
#include "pdb.h"
#include "parallel.h"

/*
 * Verify some of the invariants from verify_eqclass() for the moves
 * from one configuration within an equivalence class.  pdist is the
 * distance of p, *has_progress is set to one if we find a move that
 * lowers the distance.  Returns zero if the configuration was found
 * to be valid, 1 otherwise.  If f is not NULL, diagnostic messages
 * are printed to f if an inconsistency is found.
 */
static int
verify_configuration(patterndb pdb, tileset ts, struct puzzle *p, tileset eq,
    int pdist, int *has_progress, FILE *f)
{
	size_t i, zloc = zero_location(p), nmove = move_count(zloc);
	int dist, result = 0;
	char pstr[PUZZLE_STR_LEN];
	const signed char *moves = get_moves(zloc);

	for (i = 0; i < nmove; i++) {
		/* we already check this in the caller */
		if (tileset_has(eq, moves[i]))
			continue;

		move(p, moves[i]);
		dist = pdb[full_index(ts, p)];
		move(p, zloc);


		/* invariant 2 */
		if (abs(dist - pdist) > 1) {
			if (f != NULL) {
				puzzle_string(pstr, p);
				fprintf(f, "Move to %d has distance %u, not within 1 of %u\n%s\n",
				    moves[i], dist, pdist, pstr);
			}

			result = 1;
		}

		/* invariant 4 */
		if (pdist == dist + 1)
			*has_progress = 1;
	}

	return (result);
}

/*
 * Verify if p's entry pdist in a zero-aware pattern database pdb is
 * internally consistent with the remaining entries, checking the whole
 * equivalence class of p.  The following invariants must hold:
 *
 * 1. no entry has distance INFINITY as each configuration can be solved
 * 2. each configuration directly reachable from p's equivalence class
 *    has a distance that differs by at most 1 from p's distance
 * 3. all configurations in the same equivalence class have the same
 *    distance
 * 4. there must be a configuration whose distance is exactly one lower
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
verify_eqclass(patterndb pdb, tileset ts, struct puzzle *p, int pdist, FILE *f)
{
	size_t zloc = zero_location(p);
	tileset eq = tileset_eqclass(ts, p), map;
	int dist, result = 0, has_progress = 0;
	char pstr[PUZZLE_STR_LEN];

	/* invariant 1 */
	if (pdist == INFINITY) {
		if (f != NULL) {
			puzzle_string(pstr, p);
			fprintf(f, "Configuration has distance INFINITY:\n%s\n", pstr);
		}

		return (1);
	}

	/* quick exit so we consider each equivalence class only once */
	if (!tileset_is_canonical(ts, eq, p))
		return (0);

	/* verify all positions in the same equivalence class */
	for (map = eq; !tileset_empty(map); map = tileset_remove_least(map)) {
		move(p, tileset_get_least(map));

		if (tileset_has(ts, ZERO_TILE)) {
			dist = pdb[full_index(ts, p)];
		} else
			dist = pdist;

		/* invariant 3 */
		if (dist != pdist) {
			if (f != NULL) {
				puzzle_string(pstr, p);
				fprintf(f, "Same equivalence class but"
				    " distances %u != %u\n%s\n",
				    dist, pdist, pstr);

			}

			move(p, zloc);
			if (f != NULL) {
				puzzle_string(pstr, p);
				fprintf(f, "%s\n", pstr);
			}

			result = 1;
			continue;
		}

		result |= verify_configuration(pdb, ts, p, eq, dist, &has_progress, f);
		move(p, zloc);
	}

	/* invariant 4 */
	if (has_progress == 0 && pdist != 0) {
		if (f != NULL) {
			puzzle_string(pstr, p);
			fprintf(f, "No progress possible from configuration with distance %d:\n%s\n",
			    pdist, pstr);
		}

		return (1);
	}

	return (result);
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
 * Verify one chunk sized n entries starting at i0 of the PDB.  Return 0
 * if the chunk was found to be consistent, 1 otherwise.
 */
static void
verify_chunk(void *cfgarg, cmbindex i0, cmbindex n)
{
	struct verify_config *cfg = cfgarg;
	tileset ts = cfg->pcfg.ts;
	patterndb pdb = cfg->pcfg.pdb;

	struct puzzle p;
	struct index idx;
	cmbindex i;
	int result = 0;

	for (i = i0; i < i0 + n; i++) {
		split_index(ts, &idx, i);
		invert_index(ts, &p, &idx);
		result |= verify_eqclass(pdb, ts, &p, pdb[i], cfg->f);
	}

	cfg->result |= result;
}

/*
 * Verify an entire pattern database by verifying each configuration.
 * If f is not NULL, inconsistencies are printed to f.  For further
 * details on the verification process, read the comment above the
 * function verify_zero_position().  This function returns zero if the
 * pattern database was found to be consistent, nonzero otherwise.
 */
extern int
verify_patterndb(patterndb pdb, tileset ts, FILE *f)
{
	struct verify_config cfg;

	cfg.pcfg.pdb = pdb;
	cfg.pcfg.ts = ts;
	cfg.pcfg.worker = verify_chunk;
	cfg.result = 0;
	cfg.f = f;

	pdb_iterate_parallel(&cfg.pcfg);

	return (cfg.result);
}
