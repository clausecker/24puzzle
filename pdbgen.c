/*-
 * Copyright (c) 2017 Robert Clausecker. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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
 * Update the PDB for configuration p by finding all positions we can
 * move to from the equivalence class represented by idx that are marked
 * as UNREACHED and then setting them to round.
 */
static void
update_pdb_entry(struct patterndb *pdb, struct puzzle *p,
    const struct move *moves, size_t n_move, int round)
{
	struct index dist[MAX_MOVES];
	size_t i;

	for (i = 0; i < n_move; i++) {
		move(p, moves[i].zloc);
		move(p, moves[i].dest);

		compute_index(&pdb->aux, dist + i, p);

		move(p, moves[i].zloc);
		pdb_prefetch(pdb, dist + i);
	}

	for (i = 0; i < n_move; i++)
		pdb_conditional_update(pdb, dist + i, round);
}

/*
 * Configuration for generate_patterndb.  count is the total number of
 * entries updated in this round.
 */
struct pdbgen_config {
	struct parallel_config pcfg;
	_Atomic size_t count;
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
	struct puzzle p;
	size_t n_eqclass = eqclass_count(&pdb->aux, idx->maprank),
	    n_move, count = 0;
	int round = cfg->round;
	tileset map = tileset_unrank(pdb->aux.n_tile, idx->maprank);

	/*
	 * Every move flips the parity of the partial configuration's
	 * map.  Thus, when generating tables, we only need to scan
	 * through half the table in each round.  This saves us a lot of
	 * time.
	 */
	if ((tileset_parity(map) ^ pdb->aux.solved_parity) == (round & 1))
		return;

	invert_index_map(&pdb->aux, &p, idx);

	for (idx->eqidx = 0; idx->eqidx < n_eqclass; idx->eqidx++) {
		n_move = generate_moves(moves, eqclass_from_index(&pdb->aux, idx));
		for (idx->pidx = 0; idx->pidx < pdb->aux.n_perm; idx->pidx++)
			if (pdb_lookup(pdb, idx) == round - 1) {
				count++;
				invert_index_rest(&pdb->aux, &p, idx);
				update_pdb_entry(pdb, &p, moves, n_move, round);
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

	do {
		cfg.count = 0;
		cfg.round++;
		pdb_iterate_parallel(&cfg.pcfg);
		if (f != NULL)
			fprintf(f, "%3d: %20zu\n", cfg.round - 1, cfg.count);
	} while (cfg.count != 0);

	return (cfg.round);
}
