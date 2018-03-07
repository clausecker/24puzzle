/*-
 * Copyright (c) 2018 Robert Clausecker. All rights reserved.
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

/* etacount.c -- Compute eta by counting */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pdb.h"
#include "puzzle.h"
#include "parallel.h"
#include "index.h"
#include "heuristic.h"

/*
 * This function computes the partial eta value for each cohort.  The
 * search space size is set such that each cohort must be added as often
 * as the size of the corresponding zero tile region to get the eta
 * value of the whole pattern database.
 */
double *
make_cohort_etas(struct patterndb *pdb)
{
	size_t i, j, n_tables;
	size_t histogram[PDB_HISTOGRAM_LEN];
	double eta, *etas, scale;
	const atomic_uchar *table;

	// TODO: Adapt code to work with APDBs, not just ZPDBs.
	assert(tileset_has(pdb->aux.ts, ZERO_TILE));

	n_tables = eqclass_total(&pdb->aux);
	etas = malloc(n_tables * sizeof *etas);
	if (etas == NULL)
		return (NULL);

	scale = 1.0 / ((double)pdb->aux.n_perm * pdb->aux.n_maprank *
	    (TILE_COUNT - pdb->aux.n_tile));

	for (i = 0; i < n_tables; i++) {
		memset(histogram, 0, sizeof histogram);
		table = pdb->data + i * pdb->aux.n_perm;
		for (j = 0; j < pdb->aux.n_perm; j++)
			histogram[table[j]]++;

		eta = 0.0;
		for (j = 1; j <= PDB_HISTOGRAM_LEN; j++)
			eta = histogram[PDB_HISTOGRAM_LEN - j] + B * eta;

		etas[i] = eta * scale;
	}

	return (etas);
}

/*
 * Combine cohort eta vectors etas_a and etas_b into a vector containing
 * partial eta values for all possible combinations of the two.
 */
static double *
make_half_etas(const double *restrict etas_a, const double *restrict etas_b)
{
	struct patterndb *pdbdummy;
	size_t n_tables;
	double *etas;

	pdbdummy = pdb_dummy(tileset_least(6 + 6 + 1));
	if (pdbdummy == NULL)
		return (NULL);

	n_tables = eqclass_total(&pdbdummy->aux);
	etas = malloc(n_tables * sizeof *etas);
	if (etas == NULL) {
		pdb_free(pdbdummy);
		return (NULL);
	}

	for (i = 0; i < n_tables; i++) {
		// TODO
	}

}

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-d pdbdir] [-j nproc] tileset tileset tileset tileset\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	struct heuristic heu;

	double *cohort_etas[4], *half_etas[2], eta;
	size_t i;
	int optchar;
	tileset ts, accum = EMPTY_TILESET;
	const char *pdbdir;

	while (optchar = getopt(argc, argv, "d:j:"), optchar != -1)
		switch (optchar) {
		case 'd':
			pdbdir = optarg;
			break;

		case 'j':
			pdb_jobs = atoi(optarg);
			if (pdb_jobs < 1 || pdb_jobs > PDB_MAX_JOBS) {
				fprintf(stderr, "Number of threads must be between 1 and %d\n",
				    PDB_MAX_JOBS);
				return (EXIT_FAILURE);
			}

			break;

		default:
			usage(argv[0]);
		}

	if (argc - optind != 4)
		usage(argv[0]);

	for (i = 0; i < 4; i++) {
		if (tileset_parse(&ts, argv[optind + i]) != 0) {
			fprintf(stderr, "Cannot parse tile set: %s\n", argv[optind + i]);
			return (EXIT_FAILURE);
		}

		if (tileset_count(tileset_remove(ts, ZERO_TILE)) != 6) {
			fprintf(stderr, "Tileset needs to have six tiles: %s\n", argv[optind + i]);
			return (EXIT_FAILURE);
		}

		if (!tileset_empty(tileset_remove(tileset_intersect(ts, accum), ZERO_TILE))) {
			fprintf(stderr, "Tilesets must not overlap.\n");
			return (EXIT_FAILURE);
		}

		accum = tileset_union(accum, ts);

		if (heu_open(&heu, pdbdir, ts, tileset_has(ts, ZERO_TILE) ? "zpdb" : "pdb",
		    HEU_CREATE | HEU_NOMORPH | HEU_VERBOSE) != 0) {
			perror("heu_open");
			return (EXIT_FAILURE);
		}

		assert(heu.morphism == 0);
		cohort_etas[i] = make_cohort_etas((struct patterndb *)heu.provider);
		if (cohort_etas[i] == NULL) {
			perror("make_cohort_etas");
			return (EXIT_FAILURE);
		}

		heu_free(&heu);
	}

	assert(tileset_add(accum, ZERO_TILE) == FULL_TILESET);

	half_etas[0] = make_half_etas(cohort_etas[0], cohort_etas[1]);
	if (half_etas[0] == NULL) {
		perror("make_half_etas");
		return (EXIT_FAILURE);
	}

	half_etas[1] = make_half_etas(cohort_etas[2], cohort_etas[3]);
	if (half_etas[1] == NULL) {
		perror("make_half_etas");
		return (EXIT_FAILURE);
	}

	eta = make_eta(half_etas[0], half_etas[1]);

	printf("%.18e %s %s %s %s\n", eta,
	    argv[optind], argv[optind + 1], argv[optind + 2], argv[optind + 3]);

	return (EXIT_SUCCESS);
}
