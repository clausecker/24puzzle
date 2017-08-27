/* pdbstats.c -- compute PDB statistics */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "pdb.h"

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s pdbfile\n", argv0);

	exit(EXIT_FAILURE);
}

/*
 * Gather statistics about the PDB.  Store how often each entry occured
 * in histogram.  Store the length of runs of UNASSIGNED in runs,
 * reallocating the array as needed.  Return the number of bytes read
 * from pdbfile.
 */
static off_t
gather_data(FILE *pdbfile, off_t histogram[PDB_HISTOGRAM_LEN], off_t **runs, size_t *n_runs)
{
	size_t runlen = 0;
	off_t size = 0;
	int c;

	flockfile(pdbfile);

	while (c = getc_unlocked(pdbfile), c != EOF) {
		size++;
		histogram[c]++;
		if (c != UNREACHED) {
			if (runlen >= *n_runs) {
				*runs = realloc(*runs, (runlen + 1) * sizeof **runs);
				assert(*runs != NULL);
				memset(*runs + *n_runs, 0, (runlen + 1 - *n_runs) * sizeof **runs);
				*n_runs = runlen + 1;
			}

			(*runs)[runlen]++;
			runlen = 0;
		} else
			runlen++;
	}

	funlockfile(pdbfile);
	fclose(pdbfile);
	return (size);
}

/*
 * Compute a histogram and print it.  Then, compute the entropy of the
 * PDB and print it.
 */
static void
print_histogram(off_t histogram[PDB_HISTOGRAM_LEN], off_t size)
{
	size_t i;
	double quotient = 1.0 / (double)size, entropy, bits, prob, accum = 0.0;

	printf("histogram:\n");

	for (i = 0; i < PDB_HISTOGRAM_LEN; i++) {
		if (histogram[i] == 0)
			continue;

		prob = histogram[i] * quotient;
		entropy = -log2(prob);
		bits = histogram[i] * entropy;
		accum += bits;

		printf("0x%02zx: %20llu * %6.2fb (%6.2f%%) = %23.2fb (%23.2fB)\n",
		    i, (unsigned long long)histogram[i], entropy, 100.0 * prob, bits, bits / 8);
	}

	printf("total %.2fb (%.2fB)\n\n", accum, accum / 8);
}

/*
 * Print a distribution of UNREACHED runs in the PDB.  Then compute
 * entropies and print how long a PDB with run-length encoded blanks
 * would be.  Do nothing if no UNREACHED entries occur.
 */
static void
print_runs(off_t histogram[PDB_HISTOGRAM_LEN], size_t n_pdb, off_t runs[], size_t n_runs)
{
	size_t i, run_count = 0;
	double quotient, entropy, bits, prob, run_accum = 0.0, hist_accum = 0.0;

	printf("run-lengths:\n");

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
		    i, (unsigned long long)histogram[i], entropy, 100.0 * prob, bits, bits / 8);
	}

	printf("total %.2fb (%.2fB)\n\n", hist_accum, hist_accum / 8);

	printf("sum   %.2fb + %.2fb = %.2fb (%.2fB)\n",
	    run_accum, hist_accum, run_accum + hist_accum, (run_accum + hist_accum) / 8);
}

extern int
main(int argc, char *argv[])
{
	FILE *pdbfile;
	off_t *runs = NULL, size, histogram[PDB_HISTOGRAM_LEN] = {};
	size_t n_runs = 0;

	if (argc != 2)
	usage(argv[0]);

	pdbfile = fopen(argv[1], "rb");
	if (pdbfile == NULL) {
		perror("fopen");
		return (EXIT_FAILURE);
	}


	size = gather_data(pdbfile, histogram, &runs, &n_runs);
	printf("size %zuB\n\n", size);
	print_histogram(histogram, size);
	if (histogram[UNREACHED] != 0)
		print_runs(histogram, size, runs, n_runs);

	return (EXIT_SUCCESS);
}
