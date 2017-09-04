/* qualitytest.c -- evaluate index quality */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pdb.h"
#include "index.h"
#include "puzzle.h"
#include "tileset.h"

enum { PDB_MAX_COUNT = 24, CHUNK_SIZE = 1024 };

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-i] [-j nproc] [-n n_puzzle] [-s seed] tileset ...\n", argv0);

	exit(EXIT_FAILURE);
}

/*
 * Control structure for the parallel histogram generation. histogram is
 * the histogram we accumulate our data into, n_puzzle is the number of
 * puzzles we want to check against the pattern databases, progress is
 * the number of puzzles already generated.
 */
struct qualitytest_config {
	_Atomic size_t histogram[PDB_HISTOGRAM_LEN], progress;
	size_t n_pdb, n_puzzle;
	struct patterndb **pdbs;
};

/*
 * Worker function for random_puzzle_histograms.
 */
static void *
qualitytest_worker(void *qtcfg_arg)
{
	struct qualitytest_config *qtcfg = qtcfg_arg;
	struct puzzle p;
	size_t histogram[PDB_HISTOGRAM_LEN] = {};
	size_t i, j, n, old_progress;
	int dist;

	for (;;) {
		old_progress = atomic_fetch_add(&qtcfg->progress, CHUNK_SIZE);
		if (old_progress >= qtcfg->n_puzzle)
			break;

		n = qtcfg->n_puzzle - old_progress;
		if (n > CHUNK_SIZE)
			n = CHUNK_SIZE;

		for (i = 0; i < n; i++) {
			random_puzzle(&p);

			dist = 0;
			for (j = 0; j < qtcfg->n_pdb; j++)
				dist += pdb_lookup_puzzle(qtcfg->pdbs[j], &p);

			histogram[dist]++;
		}
	}

	/* write back results */
	for (i = 0; i < PDB_HISTOGRAM_LEN; i++)
		qtcfg->histogram[i] += histogram[i];

	return (NULL);
}

/*
 * Generate a histogram indicating what distance the pattern databases
 * pdbs predicted for n_puzzle random puzzles.  Use up to pdb_jobs
 * threads for the computation.
 */
static void
random_puzzle_histograms(size_t histogram[PDB_HISTOGRAM_LEN], size_t n_puzzle,
    struct patterndb **pdbs, size_t n_pdb)
{
	struct qualitytest_config qtcfg;
	pthread_t pool[PDB_MAX_JOBS];
	size_t i;
	int j, jobs = pdb_jobs, error;

	for (i = 0; i < PDB_HISTOGRAM_LEN; i++)
		qtcfg.histogram[i] = 0;

	qtcfg.progress = 0;
	qtcfg.n_pdb = n_pdb;
	qtcfg.n_puzzle = n_puzzle;
	qtcfg.pdbs = pdbs;

	/* this code is very similar to pdb_iterate_parallel() */
	if (jobs == 1) {
		qualitytest_worker(&qtcfg);
		goto copy_results;
	}

	/* spawn threads */
	for (j = 0; j < pdb_jobs; j++) {
		error = pthread_create(pool + j, NULL, qualitytest_worker, &qtcfg);
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

copy_results:
	for (i = 0; i < PDB_HISTOGRAM_LEN; i++)
		histogram[i] = qtcfg.histogram[i];
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

extern int
main(int argc, char *argv[])
{
	struct patterndb *pdbs[PDB_MAX_COUNT];
	size_t histogram[PDB_HISTOGRAM_LEN];
	size_t i, n_puzzle = 1000, n_pdb;
	unsigned seed = 0xfb0c4683;
	int optchar, identify = 0;
	tileset ts;

	while (optchar = getopt(argc, argv, "ij:n:s:"), optchar != -1)
		switch (optchar) {
		case 'i':
			identify = 1;
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
			seed = strtol(optarg, NULL, 0);
			break;

		default:
			usage(argv[0]);
		}

	n_pdb = argc - optind;
	if (n_pdb > PDB_MAX_COUNT) {
		fprintf(stderr, "Up to %d PDBs are allowed.\n", PDB_MAX_COUNT);
		return (EXIT_FAILURE);
	}

	for (i = 0; i < n_pdb; i++) {
		if (tileset_parse(&ts, argv[optind + i]) != 0) {
			fprintf(stderr, "Invalid tileset: %s\n", argv[optind + i]);
			return (EXIT_FAILURE);
		}

		if (identify)
			ts = tileset_add(ts, ZERO_TILE);

		pdbs[i] = pdb_allocate(ts);
		if (pdbs[i] == NULL) {
			perror("pdb_allocate");
			return (EXIT_FAILURE);
		}
	}

	/* split up allocation and generation so we know up front if we have enough RAM */
	for (i = 0; i < n_pdb; i++) {
		fprintf(stderr, "Generating PDB for tiles %s\n", argv[optind + i]);
		pdb_generate(pdbs[i], stderr);
		if (identify) {
			fprintf(stderr, "\nIdentifying PDB...\n");
			pdb_identify(pdbs[i]);
		}

		fputs("\n", stderr);
	}

	random_seed = seed;
	fprintf(stderr, "Looking up %zu random instances...\n\n", n_puzzle);

	random_puzzle_histograms(histogram, n_puzzle, pdbs, n_pdb);
	print_statistics(histogram, n_puzzle);

	return (EXIT_SUCCESS);
}
