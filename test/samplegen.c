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

/* samplegen.c -- generate random samples classified by distance */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <limits.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthreads.h>

#include "parallel.h"
#include "puzzle.h"
#include "compact.h"
#include "catalogue.h"

/*
 * We want our program to survive an interruption.  If this happens, we
 * finish processing the current sample and then immediately end
 * processing.
 */
static atomic_int interrupted = 0, quiet = 0;

/*
 * If we receive a signal asking us to terminate, set a flag to quickly
 * end execution.
 */
static void
interrupt_handler(int signo)
{
	interrupted = 1;
	if (!quiet)
		psignal(signo, "termination requested");
}

/*
 * Open a bunch of sample files whose names begin with prefix.  If the
 * sample files already exist, they are truncated.  Sample files for
 * distances 0 to distance_limit are opened.
 */
static void
open_sample_files(FILE *sample_files[PDB_HISTOGRAM_LEN], const char *prefix,
    int distance_limit)
{
	int i;
	char pathbuf[PATH_MAX];

	for (i = 0; i <= distance_limit; i++) {
		snprintf(pathbuf, PATH_MAX, "%s.%d", prefix, i);
		sample_files[i] = fopen(pathbuf, "wb");
		if (samples_files[i] == NULL) {
			perror(pathbuf);
			exit(EXIT_FAILURE);
		}
	}
}

struct sample_config {
	FILE **sample_files;
	struct pdb_catalogue *cat;
	size_t n_puzzle, n_saved;
	int distance_limit;

	_Atomic size_t puzzles_done;
	_Atomic size_t histogram[PDB_HISTOGRAM_LEN];
};

/*
 * One thread gathering samples.
 */
static void *
samples_worker(void *cfgarg)
{
	struct sample_config *cfg = cfgarg;
}

/*
 * Generate n_puzzle samples.  Enter all samples whose distance is not
 * larger than distance_limit into histogram.  Store up to n_saved of
 * these samples per histogram entry into sample_files.  If execution is
 * interrupted, less samples might be processed.  Return the actual
 * number of samples processed.  Operate in parallel with up to pdb_jobs
 * threads.  Use cat to determine the distance of a sample.  On error,
 * terminate the program.
 */
static size_t
generate_samples(size_t histogram[PDB_HISTOGRAM_LEN],
    FILE *sample_files[PDB_HISTOGRAM_LEN], struct pdb_catalogue *cat,
    int distance_limit, size_t n_puzzle, size_t n_saved)
{
	struct sample_config cfg;
	pthread_t pool[PDB_MAX_JOBS];
	size_t j;
	int i, jobs = pdb_jobs, error;

	cfg.sample_files = sample_files;
	cfg.cat = cat;
	cfg.n_puzzle = n_puzzle;
	cfg.n_saved = n_saved;
	cfg.distance_limit = distance_limit;

	cfg.puzzles_done = 0;
	memset((size_t *)cfg.histogram, 0, sizeof cfg.histogram);

	/* the logic below has been adapted from parallel.c */

	/* for easier debugging, don't multithread when jobs == 1 */
	if (jobs == 1) {
		samples_worker(&cfg);

		return (cfg.puzzles_done);
	}

	/* spawn threads */
	for (i = 0; i < jobs; i++) {
		error = pthread_create(pool + i, NULL, samples_worker, &cfg);
		if (error == 0)
			continue;

		errno = error;
		perror("pthread_create");

		/* if we can't spawn some threads, that's okay */
		if (i++ > 0)
			break;

		fprintf(stderr, "Couldn't create any threads, aborthing...\n");
		abort();
	}

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

	return (cfg.puzzles_done);
}

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-iq] [-n n_samples] [-N saved_samples] [-s seed]"
	    "[-l distance_limit] [-f prefix] [-j nproc] [-d pdbdir] catalogue\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	FILE *messages = stderr, *sample_files[PDB_HISTOGRAM_LEN];
	struct pdb_catalogue *cat;
	size_t n_puzzle = 1000, n_saved = SIZE_MAX, histogram[PDB_HISTOGRAM_LEN];
	int i, optchar, catflags = 0, distance_limit = PDB_HISTOGRAM_LEN - 1;
	char *pdbdir = NULL, *prefix = NULL;

	while (optchar = getopt(argc, argv, "N:d:f:ij:l:n:qs:"), optchar != -1) {
		switch (optchar) {
		case 'N':
			n_saved = strtoll(optarg, NULL, 0);
			break;

		case 'd':
			pdbdir = optarg;
			break;

		case 'f':
			prefix = optarg;
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

		case 'l':
			distance_limit = strtol(optarg, NULL, 0);
			if (distance_limit >= PDB_HISTOGRAM_LEN)
				distance_limit = PDB_HISTOGRAM_LEN - 1;

			break;

		case 'n':
			n_puzzle = strtoll(optarg, NULL, 0);
			break;

		case 'q':
			messages = NULL;
			quiet = 1;
			break;

		case 's':
			random_seed = strtoll(optarg, NULL, 0);
			break;

		default:
			usage(argv[0]);
		}

	if (argc != optind + 1)
		usage(argv[0]);

	cat = catalogue_load(argv[optind], pdbdir, catflags, messages);
	if (cat == NULL) {
		perror("catalogue_load");
		return (EXIT_FAILURE);
	}

	open_sample_files(sample_files, prefix, distance_limit);
	memset(histogram, 0, sizeof histogram);

	signal(SIGHUP, interrupt_handler);
	signal(SIGINT, interrupt_handler);
	signal(SIGQUIT, interrupt_handler);
	signal(SIGTERM, interrupt_handler);

	if (!quiet)
		fprintf(stderr, "Generating %zu samples, storing data to %s...\n",
		    n_puzzle, prefix);

	n_puzzle = generate_samples(histogram, sample_files, cat,
	    distance_limit, n_puzzle, n_saved);

	for (i = 0; i <= distance_limit; i++)
		fclose(sample_files[i]);

	write_statistics(prefix, histogram, n_puzzle);

	return (EXIT_SUCCESS);
}
