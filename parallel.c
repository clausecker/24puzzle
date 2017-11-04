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

/* parallel.c -- multi-threaded operation on pattern databases */

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include "tileset.h"
#include "index.h"
#include "parallel.h"
#include "pdb.h"

int pdb_jobs = 1;

/*
 * This function is the main function of each worker thread.  It grabs
 * chunks off the pile and calls worker() for them until no work is
 * left.
 */
static void *
parallel_worker(void *cfgarg)
{
	struct parallel_config *cfg = cfgarg;
	struct index idx;

	for (;;) {
		/* pick up chunk */
		idx.maprank = atomic_fetch_add(&cfg->nextrank, 1);

		/* any work left to do? */
		if (idx.maprank >= cfg->pdb->aux.n_maprank)
			break;

		idx.pidx = 0;
		idx.eqidx = 0;
		cfg->worker(cfgarg, &idx);
	}

	return (NULL);
}

/*
 * Iterate through the PDB in parallel.  Only the members pdb, ts, and
 * worker of cfg must be filled in, the other members are filled in by
 * the function.  If you want to pass extra data to cfg->worker, make
 * *cfg the first member of a larger structure as cfg is passed to every
 * call of worker.
 */
extern void
pdb_iterate_parallel(struct parallel_config *cfg)
{
	pthread_t pool[PDB_MAX_JOBS];

	size_t j;
	int i, jobs = pdb_jobs, error;

	cfg->nextrank = 0;

	/* for easier debugging, don't multithread when jobs == 1 */
	if (jobs == 1) {
		struct index idx;

		for (j = 0; j < cfg->pdb->aux.n_maprank; j++) {
			idx.pidx = 0;
			idx.maprank = j;
			idx.eqidx = 0;
			cfg->worker(cfg, &idx);
		}

		return;
	}

	/* spawn threads */
	for (i = 0; i < jobs; i++) {
		error = pthread_create(pool + i, NULL, parallel_worker, cfg);
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

	/* reduce count in case we couldn't create as many threads as we wanted */
	jobs = i;

	/* collect threads */
	for (i = 0; i < jobs; i++) {
		error = pthread_join(pool[i], NULL);
		if (error == 0)
			continue;

		errno = error;
		perror("pthread_join");
		abort();
	}
}
