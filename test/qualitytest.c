/* qualitytest.c -- evaluate index quality */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "catalogue.h"
#include "pdb.h"
#include "index.h"
#include "puzzle.h"
#include "tileset.h"

enum { CHUNK_SIZE = 1024 };

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-j nproc] [-n n_puzzle] [-s seed] [-d pbdir] catalogue\n", argv0);

	exit(EXIT_FAILURE);
}

/*
 * Control structure for the parallel histogram generation. histogram is
 * the histogram we accumulate our data into, n_puzzle is the number of
 * puzzles we want to check against the pattern databases, progress is
 * the number of puzzles already generated.  If transpose is 1, compute
 * the maximum of the PDB entrie of each puzzle and its transposition.
 * bestheu stores for each heuristic in the catalogue how often it was
 * the best heuristic.  onlyheu stores for each heuristic how often it
 * was the only heuristic finding the correct value for the given puzzle
 * configuration.
 */
struct qualitytest_config {
	_Atomic size_t histogram[PDB_HISTOGRAM_LEN], progress;
	_Atomic size_t bestheu[HEURISTICS_LEN], onlyheu[HEURISTICS_LEN];
	size_t n_puzzle;
	struct pdb_catalogue *cat;
	int transpose;
};

/*
 * Worker function for random_puzzle_histograms.
 */
static void *
qualitytest_worker(void *qtcfg_arg)
{
	struct qualitytest_config *qtcfg = qtcfg_arg;
	struct puzzle p;
	struct partial_hvals ph;
	size_t histogram[PDB_HISTOGRAM_LEN] = {};
	size_t bestheu[HEURISTICS_LEN] = {}, onlyheu[HEURISTICS_LEN] = {};
	size_t i, j, n, old_progress;
	unsigned dist, tdist, heumap;

	for (;;) {
		old_progress = atomic_fetch_add(&qtcfg->progress, CHUNK_SIZE);
		if (old_progress >= qtcfg->n_puzzle)
			break;

		n = qtcfg->n_puzzle - old_progress;
		if (n > CHUNK_SIZE)
			n = CHUNK_SIZE;

		for (i = 0; i < n; i++) {
			random_puzzle(&p);

			dist = catalogue_partial_hvals(&ph, qtcfg->cat, &p);
			heumap = catalogue_max_heuristics(qtcfg->cat, &ph);

			if (qtcfg->transpose) {
				transpose(&p);

				tdist = catalogue_partial_hvals(&ph, qtcfg->cat, &p);

				if (tdist > dist) {
					dist = tdist;
					heumap = 0;
				}

				if (tdist == dist)
					heumap |= catalogue_max_heuristics(qtcfg->cat, &ph);
			}

			histogram[dist]++;

			/* is only one bit set in heumap? */
			if ((heumap & heumap - 1) == 0)
				onlyheu[ctz(heumap)]++;

			for (j = 0; j < qtcfg->cat->n_heuristics; j++)
				if (heumap & 1 << j)
					bestheu[j]++;
		}
	}

	/* write back results */
	for (i = 0; i < PDB_HISTOGRAM_LEN; i++)
		qtcfg->histogram[i] += histogram[i];

	for (i = 0; i < qtcfg->cat->n_heuristics; i++) {
		qtcfg->bestheu[i] += bestheu[i];
		qtcfg->onlyheu[i] += onlyheu[i];
	}

	return (NULL);
}

/*
 * Generate a histogram indicating what distance the pattern databases
 * in qtcfg->cat predicted for qtcfg->n_puzzle random puzzles.  Use up
 * to pdb_jobs threads for the computation.  If qtcfg->transpose is
 * nonzero, lookup each puzzle and its transposition and use the
 * maximum of both entries.
 */
static void
random_puzzle_histograms(struct qualitytest_config *qtcfg)
{
	pthread_t pool[PDB_MAX_JOBS];
	int j, jobs = pdb_jobs, error;

	/* this code is very similar to pdb_iterate_parallel() */
	if (jobs == 1) {
		qualitytest_worker(qtcfg);
		return;
	}

	/* spawn threads */
	for (j = 0; j < pdb_jobs; j++) {
		error = pthread_create(pool + j, NULL, qualitytest_worker, qtcfg);
		if (error == 0)
			continue;

		errno = error;
		perror("pthread_create");

		if (j++ > 0)
			break;

		fprintf(stderr, "Couldn't create any threads, aborting...\n");
		abort();
	}

	jobs = j;

	/* collect threads */
	for (j = 0; j < jobs; j++) {
		error = pthread_join(pool[j], NULL);
		if (error == 0)
			continue;

		errno = error;
		perror("phread_join");
		abort();
	}
}

static void
print_statistics(size_t histogram[PDB_HISTOGRAM_LEN], size_t n_puzzle)
{
	double pfactor = 100.0 / n_puzzle, sfactor;
	size_t min = UNREACHED, max = 0, sum = 0, prev = 0, count = 0, i;

	for (i = 0; i < PDB_HISTOGRAM_LEN; i++)
		sum += histogram[i] * i;

	sfactor = 1.0 / sum;

	for (i = 0; i < PDB_HISTOGRAM_LEN; i++) {
		if (histogram[i] == 0)
			continue;

		if (i < min)
			min = i;

		if (i > max)
			max = i;

		count += histogram[i];
		prev += histogram[i] * i;

		printf("%3zu: %20zu  (%5.2f%%)  p(X <= h) = %6.4f\n",
			i, histogram[i], histogram[i] * pfactor, prev * sfactor);
	}

	printf("n_puzzle = %zu, min = %zu, max = %zu, avg = %.2f\n",
	    n_puzzle, min, max, (double)sum / n_puzzle);
}

static void
print_heuristics_quality(size_t bestheu[HEURISTICS_LEN], size_t onlyheu[HEURISTICS_LEN],
    size_t n_puzzle, size_t n_heu)
{
	double factor = 100.0 / n_puzzle;
	size_t i;

	for (i = 0; i < n_heu; i++)
		printf("%2zu: %20zu (%5.2f%%)  %20zu (%5.2f%%)\n",
		    i, bestheu[i], factor * bestheu[i], onlyheu[i], factor * onlyheu[i]);
}

extern int
main(int argc, char *argv[])
{
	struct pdb_catalogue *cat;
	struct qualitytest_config qtcfg;
	size_t n_puzzle = 1000;
	unsigned long long seed = random_seed;
	int optchar, transpose = 0;
	char *pdbdir = NULL;

	while (optchar = getopt(argc, argv, "d:j:n:s:t"), optchar != -1)
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

		case 'n':
			n_puzzle = strtol(optarg, NULL, 0);
			break;

		case 's':
			seed = strtoll(optarg, NULL, 0);
			break;

		case 't':
			transpose = 1;
			break;

		default:
			usage(argv[0]);
		}

	if (argc != optind + 1)
		usage(argv[0]);

	cat = catalogue_load(argv[optind], pdbdir, stderr);
	if (cat == NULL) {
		perror("catalogue_load");
		return (EXIT_FAILURE);
	}

	random_seed = seed;
	fprintf(stderr, "Looking up %zu random instances...\n\n", n_puzzle);

	memset(&qtcfg, 0, sizeof qtcfg);
	qtcfg.n_puzzle = n_puzzle;
	qtcfg.cat = cat;
	qtcfg.transpose = transpose;

	random_puzzle_histograms(&qtcfg);
	print_statistics((size_t *)qtcfg.histogram, qtcfg.n_puzzle);
	puts("");
	print_heuristics_quality((size_t *)qtcfg.bestheu, (size_t *)qtcfg.onlyheu,
	    qtcfg.n_puzzle, cat->n_heuristics);

	return (EXIT_SUCCESS);
}
