/* pdbstats.c -- compute PDB statistics */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "tileset.h"
#include "index.h"
#include "pdb.h"

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-t tile,tile,tile,...] -f file [-j nproc]\n", argv0);

	exit(EXIT_FAILURE);
}

/*
 * Compute a histogram and print it.  Then, compute the entropy of the
 * PDB and print it.
 */
static void
do_histogram(patterndb pdb, tileset ts, cmbindex histogram[PDB_HISTOGRAM_LEN])
{
	size_t i;
	double quotient = 1.0 / (double)search_space_size(ts), entropy, bits, prob, accum = 0.0;

	printf("histogram:\n");
	generate_pdb_histogram(histogram, pdb, ts);

	for (i = 0; i < PDB_HISTOGRAM_LEN; i++) {
		if (histogram[i] == 0)
			continue;

		prob = histogram[i] * quotient;
		entropy = -log2(prob);
		bits = histogram[i] * entropy;
		accum += bits;

		printf("0x%02zx: %20llu * %6.2fb (%6.2f%%) = %23.2fb (%23.2fB)\n",
		    i, histogram[i], entropy, 100.0 * prob, bits, bits / 8);
	}

	printf("total %.2fb (%.2fB)\n\n", accum, accum / 8);
}

/*
 * Print a distribution of UNREACHED runs in the PDB.  Then compute
 * entropies and print how long a PDB with run-length encoded blanks
 * would be.  Do nothing if no UNREACHED entries occur.
 */
static void
do_runs(patterndb pdb, tileset ts, cmbindex histogram[PDB_HISTOGRAM_LEN])
{
	size_t i, n_pdb = search_space_size(ts), runlen = 0;
	size_t *runs = NULL, n_runs = 0, run_count = 0;
	double quotient, entropy, bits, prob, run_accum = 0.0, hist_accum = 0.0;

	if (histogram[UNREACHED] == 0) {
		printf("No UNREACHED entries, skipping run length analysis.\n");
		return;
	}

	printf("run-lengths:\n");

	for (i = 0; i < n_pdb; i++) {
		if (pdb[i] != UNREACHED) {
			if (runlen >= n_runs) {
				runs = realloc(runs, (i + 1) * sizeof *runs);
				assert(runs != NULL);
				memset(runs + n_runs, 0, (i + 1 - n_runs) * sizeof *runs);
				n_runs = i + 1;
			}

			runs[runlen]++;
			runlen = 0;
		} else
			runlen++;
	}

	for (i = 0; i < n_runs; i++)
		run_count += runs[i];

	quotient = 1.0 / (double)run_count;
	for (i = 0; i < n_runs; i++) {
		if (runs[i] == 0)
			continue;

		prob = runs[i] * quotient;
		entropy = -log2(prob);
		bits = runs[i] * entropy;
		run_accum += bits;

		printf("%4zx: %20zu * %6.2fb (%6.2f%%) = %23.2fb (%23.2fB)\n",
		    i, runs[i], entropy, 100.0 * prob, bits, bits / 8);
	}

	printf("total %.2fb (%.2fB)\n\n", run_accum, run_accum / 8);

	quotient = 1.0 / (n_pdb - histogram[UNREACHED]);
	for (i = 0; i < PDB_HISTOGRAM_LEN - 1; i++) {
		if (histogram[i] == 0)
			continue;

		prob = histogram[i] * quotient;
		entropy = -log2(prob);
		bits = histogram[i] * entropy;
		hist_accum += bits;

		printf("0x%02zx: %20llu * %6.2fb (%6.2f%%) = %23.2fb (%23.2fB)\n",
		    i, histogram[i], entropy, 100.0 * prob, bits, bits / 8);
	}

	printf("total %.2fb (%.2fB)\n\n", hist_accum, hist_accum / 8);

	printf("sum   %.2fb + %.2fb = %.2fb (%.2fB)\n",
	    run_accum, hist_accum, run_accum + hist_accum, (run_accum + hist_accum) / 8);
}

extern int
main(int argc, char *argv[])
{
	tileset ts = 0x00000e7; /* 0 1 2 5 6 7 */
	size_t count, size;
	int optchar;
	const char *fname = NULL;
	FILE *f;
	patterndb pdb;
	cmbindex histogram[PDB_HISTOGRAM_LEN];

	while (optchar = getopt(argc, argv, "f:ij:o:t:"), optchar != -1)
		switch (optchar) {
		case 'f':
			fname = optarg;
			break;

		case 'j':
			pdb_jobs = atoi(optarg);
			if (pdb_jobs < 1 || pdb_jobs > PDB_MAX_JOBS) {
				fprintf(stderr, "Number of threads must be between 1 and %d\n",
				    PDB_MAX_JOBS);
				return (EXIT_FAILURE);
			}

			break;

		case 't':
			if (tileset_parse(&ts, optarg) != 0) {
				fprintf(stderr, "Cannot parse tile set: %s\n", optarg);
				return (EXIT_FAILURE);
			}

			break;

		case '?':
		case ':':
			usage(argv[0]);
		}

	if (tileset_count(ts) >= 16) {
		fprintf(stderr, "%d tiles are too many tiles. Up to 15 tiles allowed.\n",
		    tileset_count(ts));
		return (EXIT_FAILURE);
	}

	if (fname == NULL)
		usage(argv[0]);


	f = fopen(fname, "rb");
	if (f == NULL) {
		perror("fopen");
		return (EXIT_FAILURE);
	}

	size = search_space_size(ts);

	pdb = malloc(size);
	if (pdb == NULL) {
		perror("malloc");
		return (EXIT_FAILURE);
	}

	count = fread(pdb, 1, size, f);
	if (count < size) {
		if (ferror(f))
			perror("fread");
		else
			fprintf(stderr, "PDB too short.\n");

		return (EXIT_FAILURE);
	}

	printf("size %zuB\n\n", size);

	do_histogram(pdb, ts, histogram);
	do_runs(pdb, ts, histogram);

	return (EXIT_SUCCESS);
}
