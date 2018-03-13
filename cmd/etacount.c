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
 * resulting partial eta values are unscaled, instead the result of
 * the computation is scaled at the end.
 */
double *
make_cohort_etas(struct patterndb *pdb)
{
	size_t i, j, n_tables;
	size_t histogram[PDB_HISTOGRAM_LEN];
	double eta, *etas;
	const atomic_uchar *table;

	// TODO: Adapt code to work with APDBs, not just ZPDBs.
	assert(tileset_has(pdb->aux.ts, ZERO_TILE));

	n_tables = eqclass_total(&pdb->aux);
	etas = malloc(n_tables * sizeof *etas);
	if (etas == NULL)
		return (NULL);

	for (i = 0; i < n_tables; i++) {
		memset(histogram, 0, sizeof histogram);
		table = pdb->data + i * pdb->aux.n_perm;
		for (j = 0; j < pdb->aux.n_perm; j++)
			histogram[table[j]]++;

		eta = 0.0;
		for (j = 1; j <= PDB_HISTOGRAM_LEN; j++)
			eta = histogram[PDB_HISTOGRAM_LEN - j] + eta * (1.0 / B);

		etas[i] = eta;
	}

	return (etas);
}

/*
 * Compute the partial eta for all possible subdivisions of map (a tileset
 * of 12 tiles), assuming the zero tile is at zloc.  Return the computed
 * partial eta.  aux6 is a pointer to an index_aux structure for a six
 * tiles ZPDB.
 */
double
single_map_eta(const double *restrict etas_a, const double *restrict etas_b,
    tileset map, unsigned zloc, struct index_aux *aux6)
{
	double eta = 0.0;
	size_t i, off_a, off_b;
	tileset ts_a, ts_b;
	tsrank rank_a, rank_b;
	enum { SIX_OF_TWELVE = 924 }; /* 12 choose 6 */

	for (i = 0; i < SIX_OF_TWELVE; i++) {
		ts_a = pdep(map, tileset_unrank(6, i));
		ts_b = tileset_difference(map, ts_a);

		rank_a = tileset_rank(ts_a);
		rank_b = tileset_rank(ts_b);

		off_a = aux6->idxt[rank_a].offset + aux6->idxt[rank_a].eqclasses[zloc];
		off_b = aux6->idxt[rank_b].offset + aux6->idxt[rank_b].eqclasses[zloc];

		eta += etas_a[off_a] * etas_b[off_b];
	}

	return (eta);
}

/*
 * Combine cohort eta vectors etas_a and etas_b into a vector containing
 * partial eta values for all possible combinations of the two.  pdbdummy
 * is a pointer to an arbitrary 12 tile dummy ZPDB.
 */
static double *
make_half_etas(const double *restrict etas_a, const double *restrict etas_b,
    struct patterndb *pdbdummy)
{
	struct index_aux aux6;
	struct index idx;
	size_t n_eqclass, n_tables, offset;
	double *etas;
	tileset map;

	/* it doesn't really matter which tile set we use as long as it has 6 tiles */
	make_index_aux(&aux6, tileset_least(6 + 1));

	n_tables = eqclass_total(&pdbdummy->aux);
	etas = malloc(n_tables * sizeof *etas);
	if (etas == NULL) {
		pdb_free(pdbdummy);
		return (NULL);
	}

	idx.pidx = 0;
	for (idx.maprank = 0; idx.maprank < pdbdummy->aux.n_maprank; idx.maprank++) {
		map = tileset_unrank(12, idx.maprank);
		n_eqclass = eqclass_count(&pdbdummy->aux, idx.maprank);
		for (idx.eqidx = 0; idx.eqidx < n_eqclass; idx.eqidx++) {
			offset = pdbdummy->aux.idxt[idx.maprank].offset + idx.eqidx;
			etas[offset] = single_map_eta(etas_a, etas_b, map,
			    canonical_zero_location(&pdbdummy->aux, &idx), &aux6);
		}
	}

	return (etas);
}

/*
 * Combine the contents of eta arrays half_etas[0] and half_etas[1] into
 * a single eta.  pdbdummy is a pointer to an arbitrary 12 tile ZPDB.
 */
static double
make_eta(const double *restrict etas_a, const double *restrict etas_b,
    struct patterndb *pdbdummy)
{
	struct index_aux *aux = &pdbdummy->aux;
	struct index idx;
	double eta = 0.0, sum;
	size_t off_a, off_b;
	unsigned zloc;
	tsrank rank_a, rank_b;
	tileset map_a, map_b, cmap;

	idx.pidx = 0;
	for (idx.maprank = 0; idx.maprank < aux->n_maprank; idx.maprank++) {
		sum = 0.0;
		map_a = tileset_unrank(12, idx.maprank);

		for (cmap = tileset_complement(map_a); !tileset_empty(cmap);
		    cmap = tileset_remove_least(cmap)) {
			zloc = tileset_get_least(cmap);
			map_b = tileset_remove(tileset_complement(map_a), zloc);

			rank_a = idx.maprank;
			rank_b = tileset_rank(map_b);

			off_a = aux->idxt[rank_a].offset + aux->idxt[rank_a].eqclasses[zloc];
			off_b = aux->idxt[rank_b].offset + aux->idxt[rank_b].eqclasses[zloc];

			sum += etas_a[off_a] * etas_b[off_b];
		}

		/* increase precision slightly by keeping track of sum separately */
		eta += sum;
	}

	/* scale by 25! */
	return (eta * 6.446950284384474737e-26);
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
	struct patterndb *pdbdummy;

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

	pdbdummy = pdb_dummy(tileset_least(6 + 6 + 1));
	if (pdbdummy == NULL) {
		perror("pdb_dummy");
		return (EXIT_FAILURE);
	}

	assert(pdbdummy->aux.n_maprank == 5200300);

	fprintf(stderr, "Joining eta values for %s and %s\n", argv[optind + 0], argv[optind + 1]);
	half_etas[0] = make_half_etas(cohort_etas[0], cohort_etas[1], pdbdummy);
	if (half_etas[0] == NULL) {
		perror("make_half_etas");
		return (EXIT_FAILURE);
	}

	fprintf(stderr, "Joining eta values for %s and %s\n", argv[optind + 2], argv[optind + 3]);
	half_etas[1] = make_half_etas(cohort_etas[2], cohort_etas[3], pdbdummy);
	if (half_etas[1] == NULL) {
		perror("make_half_etas");
		return (EXIT_FAILURE);
	}

	fprintf(stderr, "Joining halves\n");
	eta = make_eta(half_etas[0], half_etas[1], pdbdummy);

	printf("%.18e %s %s %s %s\n", eta,
	    argv[optind], argv[optind + 1], argv[optind + 2], argv[optind + 3]);

	return (EXIT_SUCCESS);
}
