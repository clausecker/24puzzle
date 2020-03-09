/*-
 * Copyright (c) 2018--2019 Robert Clausecker. All rights reserved.
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

/* walkdist.c -- estimate distance distributions from random walks */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "search.h"
#include "catalogue.h"
#include "pdb.h"
#include "index.h"
#include "puzzle.h"
#include "tileset.h"
#include "random.h"

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-iht] [-j nproc] [-d pdbdir] [-m fsmfile] [-n n_puzzle] [-s seed] catalogue distance\n", argv0);

	exit(EXIT_FAILURE);
}

static void
collect_walks(size_t samples[SEARCH_PATH_LEN], size_t *failures, int steps,
    size_t n_puzzle, struct pdb_catalogue *cat, const struct fsm *fsm,
    int give_heu)
{
	struct puzzle p;
	struct path path;
	size_t i = 0;

	while (i < n_puzzle) {
		p = solved_puzzle;
		if (random_walk(&p, steps, fsm) == 0) {
			++*failures;
			continue;
		}

		if (give_heu)
			samples[catalogue_hval(cat, &p)]++;
		else {
			search_ida(cat, fsm, &p, &path, NULL, NULL, 0);
			assert(path.pathlen != SEARCH_NO_PATH);
			samples[path.pathlen]++;
		}

		i++;
	}
}

static void
print_statistics(size_t samples[SEARCH_PATH_LEN], size_t failures, int steps, size_t n_puzzle)
{
	double n_quot = 1.0 / n_puzzle;
	size_t i, len = 0;

	printf("%d %zu %zu", steps, n_puzzle, failures);

	/* find last nonzero entry in samples */
	for (i = 0; i < SEARCH_PATH_LEN; i++)
		if (samples[i] != 0)
			len = i + 1;

	for (i = 0; i < len; i++)
		printf(" %g", samples[i] * n_quot);

	printf("\n");
}

extern int
main(int argc, char *argv[])
{
	const struct fsm *fsm = &fsm_simple;
	struct pdb_catalogue *cat;
	FILE *fsmfile;
	size_t samples[SEARCH_PATH_LEN], n_puzzle = 1000, failures = 0;
	int optchar, catflags = 0, give_heu = 0, steps, transpose = 0;
	char *pdbdir = NULL;

	while (optchar = getopt(argc, argv, "d:ij:hm:n:s:t"), optchar != -1)
		switch (optchar) {
		case 'd':
			pdbdir = optarg;
			break;

		case 'i':
			catflags |= CAT_IDENTIFY;
			break;

		case 'j':
			pdb_jobs = atoi(optarg);
			if (pdb_jobs < 1 || pdb_jobs > PDB_MAX_JOBS) {
				fprintf(stderr, "Number of threads must be between 1 and %d\n",
				    PDB_MAX_JOBS);
				return (EXIT_FAILURE);
			}

			break;

		case 'h':
			give_heu = 1;
			break;

		case 'm':
			fprintf(stderr, "Loading finite state machine file %s\n", optarg);
			fsmfile = fopen(optarg, "rb");
			if (fsmfile == NULL) {
				perror(optarg);
				return (EXIT_FAILURE);
			}

			fsm = fsm_load(fsmfile);
			if (fsm == NULL) {
				perror("fsm_load");
				return (EXIT_FAILURE);
			}

			fclose(fsmfile);
			break;

		case 'n':
			n_puzzle = strtol(optarg, NULL, 0);
			break;

		case 's':
			random_seed = strtoll(optarg, NULL, 0);
			break;

		case 't':
			transpose = 1;
			break;

		default:
			usage(argv[0]);
		}

	if (argc != optind + 2)
		usage(argv[0]);

	steps = (int)strtol(argv[optind + 1], NULL, 0);
	if (steps < 0) {
		fprintf(stderr, "Number of steps cannot be negative: %s\n", argv[optind + 1]);
		usage(argv[0]);
	}

	cat = catalogue_load(argv[optind], pdbdir, catflags, stderr);
	if (cat == NULL) {
		perror("catalogue_load");
		return (EXIT_FAILURE);
	}

	if (transpose && catalogue_add_transpositions(cat) != 0) {
		perror("catalogue_add_transpositions");
		fprintf(stderr, "Proceeding anyway...\n");
	}

	memset(samples, 0, sizeof samples);

	collect_walks(samples, &failures, steps, n_puzzle, cat, fsm, give_heu);
	print_statistics(samples, failures, steps, n_puzzle);

	return (EXIT_SUCCESS);
}
