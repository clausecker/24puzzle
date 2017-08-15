/* genpdb.c -- generate pattern databases */

#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "puzzle.h"
#include "tileset.h"
#include "index.h"
#include "pdb.h"

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
 * Generate one chunk of one round of the PDB where round > 0.  The
 * chunk starts at index i0 and is n entries long.  The caller must
 * ensure that the PDB is not read out of bounds.  This function
 * returns the number of PDB entries updated.  It is safe to execute
 * in parallel on the same dataset.
 */
static cmbindex
generate_round_chunk(patterndb pdb, tileset ts, int round, cmbindex i0, cmbindex n)
{
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

	return (count);
}

/*
 * This structure controls one worker thread.  Each thread generates the
 * PDB in chunks of PDB_CHUNK_SIZE.  This is done instead of splitting
 * the PDB into j chunks for j threads evenly as the work is far from
 * being equidistributed in the table.
 */
struct worker_configuration {
	_Atomic cmbindex count;
	_Atomic cmbindex offset;
	const cmbindex size;
	const patterndb pdb;
	const tileset ts;
	const int round;
};

/*
 * This function is the main function of each worker thread.  It grabs
 * chunks off the pile and updates the PDB for them until no work is
 * left.  The number of elements updated is written to cfgarg->count.
 */
static void *
genpdb_worker(void *cfgarg)
{
	struct worker_configuration *cfg = cfgarg;
	cmbindex i, n;

	for (;;) {
		/* pick up chunk */
		i = atomic_fetch_add(&cfg->offset, PDB_CHUNK_SIZE);

		/* any work left to do? */
		if (i >= cfg->size)
			break;

		n = i + PDB_CHUNK_SIZE <= cfg->size ? PDB_CHUNK_SIZE : cfg->size - i;
		cfg->count += generate_round_chunk(cfg->pdb, cfg->ts, cfg->round, i, n);
	}

	return (NULL);
}

/*
 * Generate one round of the PDB where round > 0.  Use jobs threads.
 * This function returns the number of PDB entries updated.  It is
 * assumed that 0 < i <= GENPDB_MAX_THREADS.
 */
static cmbindex
generate_round(patterndb pdb, tileset ts, int round, int jobs)
{
	pthread_t pool[PDB_MAX_THREADS];
	struct worker_configuration cfg = {
		.count = 0,
		.offset = 0,
		.size = search_space_size(ts),
		.pdb = pdb,
		.ts = ts,
		.round = round
	};

	int i, actual_jobs, error;

	/* for easier debugging, don't multithread when jobs == 1 */
	if (jobs == 1) {
		genpdb_worker(&cfg);
		return (cfg.count);
	}

	/* spawn threads */
	for (i = 0; i < jobs; i++) {
		error = pthread_create(pool + i, NULL, genpdb_worker, &cfg);
		if (error == 0)
			continue;

		errno = error;
		perror("pthread_create");

		/*
		 * if we cannot spawn as many threads as we like
		 * but we can at least spawn some threads, just keep
		 * going with the tablebase generation.  Otherwise,
		 * there is no point in going on, so this is a good spot
		 * to throw our hands up in the air and give up.
		 */
		if (i++ > 0)
			break;

		fprintf(stderr, "Couldn't create any threads, aborting...\n");
		abort();
	}

	actual_jobs = i;

	/* collect threads */
	for (i = 0; i < actual_jobs; i++) {
		error = pthread_join(pool[i], NULL);
		if (error == 0)
			continue;

		errno = error;
		perror("pthread_join");
		abort();
	}

	return (cfg.count);
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
generate_patterndb(patterndb pdb, tileset ts, int jobs, FILE *f)
{
	cmbindex count;
	int round = 0;

	count = setup_pdb(pdb, ts);
	if (f != NULL)
		fprintf(f, "  0: %20llu\n", count);

	do {
		count = generate_round(pdb, ts, ++round, jobs);
		if (f != NULL)
			fprintf(f, "%3d: %20llu\n", round, count);
	} while (count != 0);

	return (round);
}
