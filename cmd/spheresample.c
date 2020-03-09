/*-
 * Copyright (c) 2020 Robert Clausecker. All rights reserved.
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

/* spheresample.c -- generate spherical samples */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "catalogue.h"
#include "fsm.h"
#include "random.h"
#include "search.h"
#include "statistics.h"

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-v] [-d pdbdir] [-m fsmfile] [-n n_puzzle] -o outfile [-s seed] catalogue distance\n", argv0);

	exit(EXIT_FAILURE);
}

/*
 * The current state of the sampling process.  This is updated once for
 * every puzzle and keeps track of how far we are done.
 */
struct samplestate {
	long long n_puzzle;	/* total number of puzzles */
	long long n_samples;	/* puzzles generated */
	long long n_accepted;	/* puzzles with the right distance */
	long long n_aborted;	/* aborted random walks */
	double harmonic_sum;	/* harmonic sum of probabilities for accepted puzzles */
};

/*
 * payload for search_ida to be used by add_solution.
 */
struct payload {
	const struct fsm *fsm;	/* fsm that was used in the random walk */
	double prob;		/* cumulative probability */
	int n_solution;		/* number of solutions */
	int zloc;		/* original zero-tile location */
};


/*
 * Verify that pa would not have been pruned by plarg.  If
 * it would not be pruned, compute the probability of having
 * followed this path under plarg and add it to pl->prob.
 */
static void
add_solution(const struct path *pa, void *plarg)
{
	struct payload *pl = (struct payload *)plarg;

	/* TODO */
}

/*
 * Write puzzle p with probability prob to sample file prob.  Note that
 * this sample file has a different layout than the ones used in the
 * other programs.
 */
static void
write_entry(FILE *outfile, const struct puzzle *p, double prob)
{
	struct sample s;
	size_t count;

	memset(&s, 0, sizeof s); /* clear padding if any */
	pack_puzzle(&s.cp, p);
	s.p = prob;

	count = fwrite(&s, sizeof s, 1, outfile);
	if (count != 1) {
		perror("write_entry");
		exit(EXIT_FAILURE);
	}
}

/*
 * Report the current state of the sample-taking process to stderr.
 * If we write to a tty, each line is prefixed with \r, otherwise each
 * line ends in \n.
 */
static void
print_state(const struct samplestate *state)
{
	/* TODO */
}

/*
 * Take state.n_puzzle samples at steps steps using fsmfile for pruning
 * and write them to outfile.  Use cat as an aid to solve the puzzle.
 * If verbose is set, print status information every now and then.
 */
static void
take_samples(FILE *outfile, struct samplestate *state, const struct fsm *fsm,
    struct pdb_catalogue *cat, int steps, int verbose)
{
	struct path pa;
	struct puzzle p;
	struct payload pl;

	pl.fsm = fsm;

	for (; state->n_samples < state->n_puzzle; state->n_samples++) {
		p = solved_puzzle;
		if (!random_walk(&p, steps, fsm)) {
			state->n_aborted++;
			continue;
		}

		pl.prob = 0.0;
		pl.n_solution = 0;
		pl.zloc = zero_location(&p);

		search_ida(cat, &fsm_simple, &p, &pa, add_solution, &pl, IDA_LAST_FULL);
		if (pa.pathlen != steps)
			continue;

		state->n_accepted++;
		state->harmonic_sum += 1.0 / pl.prob;
		write_entry(outfile, &p, pl.prob);

		if (verbose)
			print_state(state);
	}
}

extern int
main(int argc, char *argv[])
{
	struct samplestate state = { 1000, 0, 0, 0, 0.0 };
	const struct fsm *fsm = &fsm_simple;
	struct pdb_catalogue *cat;
	FILE *fsmfile, *prelimfile, *outfile = NULL;
	int optchar, verbose = 0, steps;
	char *pdbdir = NULL;

	while (optchar = getopt(argc, argv, "d:m:n:o:s:v"), optchar != -1)
		switch (optchar) {
		case 'd':
			pdbdir = optarg;
			break;

		case 'm':
			fsmfile = fopen(optarg, "rb");
			if (fsmfile == NULL) {
				perror(optarg);
				return (EXIT_FAILURE);
			}

			fsm = fsm_load(fsmfile);
			if (fsm == NULL)  {
				perror("fsm_load");
				return (EXIT_FAILURE);
			}

			fclose(fsmfile);
			break;

		case 'n':
			state.n_puzzle = strtol(optarg, NULL, 0);
			break;

		case 'o':
			outfile = fopen(optarg, "wb");
			if (outfile == NULL) {
				perror(optarg);
				return (EXIT_FAILURE);
			}

			break;

		case 's':
			random_seed = strtoll(optarg, NULL, 0);
			break;

		case 'v':
			verbose = 1;

		default:
			usage(argv[0]);
		}

	if (argc != optind + 2)
		usage(argv[0]);

	if (outfile == NULL) {
		fprintf(stderr, "Mandatory option -o outfile omitted\n");
		usage(argv[0]);
	}

	steps = (int)strtol(argv[optind + 1], NULL, 0);
	if (steps < 0) {
		fprintf(stderr, "Number of steps cannot be negative: %s\n", argv[optind + 1]);
		usage(argv[0]);
	}

	cat = catalogue_load(argv[optind], pdbdir, 0, verbose ? stderr : NULL);
	if (cat == NULL) {
		perror("catalogue_load");
		return (EXIT_FAILURE);
	}

	/* file for samples before they are fixed up */
	prelimfile = tmpfile();
	if (prelimfile == NULL) {
		perror("tmpfile");
		return (EXIT_FAILURE);
	}

	take_samples(prelimfile, &state, fsm, cat, steps, verbose);
	/* TODO: add code to fix up probabilities */
	/* fix_up(outfile, prelimfile, &state); */
}
