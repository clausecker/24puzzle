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

#include <math.h>
#include <pthread.h>

#include "catalogue.h"
#include "fsm.h"
#include "pdb.h"
#include "random.h"
#include "search.h"
#include "statistics.h"

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-vr] [-d pdbdir] [-j nproc] [-m fsmfile] [-n n_puzzle] [-N n_written] -o outfile [-s seed] catalogue distance\n", argv0);

	exit(EXIT_FAILURE);
}

/*
 * The current state of the sampling process.  This is updated once for
 * every puzzle and keeps track of how far we are done.
 */
struct samplestate {
	/* members that may not be concurrently modified */
	FILE *outfile;		/* temporary file for sample data */
	const struct fsm *fsm;	/* finite state machine for sampling */
	struct pdb_catalogue *cat; /* catalogue for searching */
	long long n_puzzle;	/* total number of puzzles */
	int verbose;		/* whether we want to print a status */

	/* members that may only be accessed when lock is held */
	pthread_mutex_t lock;	/* guards all members below* */
	long long n_samples;	/* puzzles generated */
	long long n_accepted;	/* puzzles with the right distance */
	long long n_aborted;	/* aborted random walks */
	long long path_sum;	/* sum of solution numbers */
	double size_sum;	/* sum of reciprocal probabilities */
	int steps;		/* number of steps to walk */
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
 * Transition by moving the zero tile to move.  Compute the reciprocal
 * probability of this having happened if fsm was used.  Update st
 * appropriately.  Return the probability or zero if it could not have
 * happened.
 */
static double
add_step(struct fsm_state *st, const struct fsm *fsm, int move, int budget)
{
	int n_legal = 0;
	signed char moves[4];

	/* we can't proceed from a match state, so rule that out */
	if (fsm_is_match(*st))
		return (0.0);

	/* count the number of moves fsm would allow out of this situation */
	n_legal = fsm_get_moves_moribund(moves, *st, fsm, budget);
	assert(0 <= n_legal && n_legal <= 4);

	/* update st according to m */
	*st = fsm_advance(fsm, *st, move);

	return (n_legal);
}

/*
 * Verify that pa would not have been pruned by plarg.  If
 * it would not be pruned, compute the probability of having
 * followed this path under plarg and add it to pl->prob.
 */
static void
add_solution(const struct path *pa, void *plarg)
{
	struct payload *pl = (struct payload *)plarg;
	struct fsm_state st;
	double prob = 1.0;
	size_t i;

	/* special case: can't access pa->moves[pa->pathlen - 1] if pa->pathlen == 0 */
	if (pa->pathlen == 0) {
		pl->prob += 1.0;
		pl->n_solution++;

		return;
	}

	st = fsm_start_state(pa->moves[pa->pathlen - 1]);
	assert(pa->moves[pa->pathlen - 1] == zero_location(&solved_puzzle));

	/*
	 * the path records moves to solve the puzzle, but we want to do
	 * opposite: generate a puzzle configuration from the solved
	 * puzzle.  To do so, the moves are executed in reverse order,
	 * swapping source and destination.  As the source of each move
	 * is the destination of the previous move, this is achieved by
	 * leaving out the first move and appending a final move to
	 * reach the solved configuration.
	 */
	for (i = 0; i < pa->pathlen - 1; i++)
		prob *= add_step(&st, pl->fsm, pa->moves[pa->pathlen - i - 2], pa->pathlen - i);

	/* finish undoing the solution */
	prob *= add_step(&st, pl->fsm, pl->zloc, 1);

	if (!fsm_is_match(st)) {
		assert(prob != 0.0);
		pl->prob += 1.0 / prob;
		pl->n_solution++;
	}
}

/*
 * Write puzzle p with probability prob to sample file prob.  Note that
 * this sample file has a different layout than the ones used in the
 * other programs.
 */
static void
write_sample(FILE *outfile, const struct puzzle *p, double prob)
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
	static const char *fmt = NULL;
	long long total, samples, accepted, aborted, failed;
	double ratio, size;

	if (fmt == NULL)
		fmt = isatty(fileno(stderr))
		    ? "\r%5.2f%% acc %5.2f%% fail %5.2f%% abort %5.2f%% done %#g size"
		    :   "%5.2f%% acc %5.2f%% fail %5.2f%% abort %5.2f%% done %#g size\n";

	total = state->n_puzzle;
	samples = state->n_samples;
	accepted = state->n_accepted;
	aborted = state->n_aborted;
	failed = samples - accepted - aborted;
	ratio = 100.0 / samples;
	size = state->size_sum / state->n_samples;
	fprintf(stderr, fmt, accepted * ratio, failed * ratio, aborted * ratio,
	    (100.0 * samples) / total, size);
}

/*
 * Take state->n_puzzle samples at state->steps steps using
 * state->fsmfile for pruning and write them to state->outfile.  Use
 * state->cat as an aid to solve the puzzle.  If state->verbose is
 * set, print status information every now and then.  This function
 * always returns NULL for compatibility with pthread_create.
 */
static void *
take_samples(void *starg)
{
	struct samplestate *state = (struct samplestate *)starg;
	struct path pa;
	struct puzzle p;
	struct payload pl;
	int error, success;

	pl.fsm = state->fsm;

	error = pthread_mutex_lock(&state->lock);
	if (error != 0) {
		fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(error));
		abort();
	}

	for (; state->n_samples < state->n_puzzle; state->n_samples++) {
		/* print state every once in a while */
		if (state->verbose && state->n_samples % 1000 == 0)
			print_state(state);

		p = solved_puzzle;
		if (!random_walk(&p, state->steps, state->fsm)) {
			state->n_aborted++;
			continue;
		}

		error = pthread_mutex_unlock(&state->lock);
		if (error != 0) {
			fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(error));
			abort();
		}

		pl.prob = 0.0;
		pl.n_solution = 0;
		pl.zloc = zero_location(&p);

		search_ida(state->cat, &fsm_simple, &p, &pa, add_solution, &pl, IDA_LAST_FULL);
		assert(pa.pathlen <= state->steps);
		success = pa.pathlen == state->steps;

		if (success)  {
			/* we came there some way, so we should always find a solution */
			assert(pl.n_solution > 0);

			/*
			 * A FILE structure has its own lock, so this can be
			 * done without holding state->lock.
			 */
			write_sample(state->outfile, &p, pl.prob);
		}

		error = pthread_mutex_lock(&state->lock);
		if (error != 0) {
			fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(error));
			abort();
		}

		if (success) {
			state->n_accepted++;
			state->path_sum += pl.n_solution;
			state->size_sum += 1.0 / pl.prob;
		}
	}

	error = pthread_mutex_unlock(&state->lock);
	if (error != 0) {
		fprintf(stderr, "pthread_mutex_unlock: %s\n", strerror(error));
		abort();
	}

	return (NULL);
}

/*
 * Take state->n_puzzle samples using up to pdb_jobs threads in
 * parallel.  Otherwise same as take_samples (which does the
 * heavy lifting).
 */
static void
take_samples_parallel(struct samplestate *state)
{
	/* shamelessly ripped from parallel.c */
	pthread_t pool[PDB_MAX_JOBS];

	int i, jobs, error;

	jobs = pdb_jobs;

	/* for easier debugging, don't multithread when jobs == 1 */
	if (jobs == 1) {
		take_samples(state);
		goto end;
	}

	/* spawn threads */
	for (i = 0; i < jobs; i++) {
		error = pthread_create(pool + i, NULL, take_samples, state);
		if (error != 0) {
			/* accept less threads, but not none */
			if (i > 0)
				break;

			fprintf(stderr, "pthread_create: %s\n", strerror(error));
			exit(EXIT_FAILURE);
		}
	}

	jobs = i;

	/* collect threads */
	for (i = 0; i < jobs; i++) {
		error = pthread_join(pool[i], NULL);
		if (error != 0)
			fprintf(stderr, "pthread_join: %s\n", strerror(error));
	}

	/* print final state */
end:	if (state->verbose) {
		print_state(state);

		/* end line if tty which print_state does not */
		if (isatty(fileno(stderr)))
			fprintf(stderr, "\n");
	}
}

/*
 * after initial sampling, the probabilities need to be adjusted to be
 * relative to the chance of hitting an accepted sample, not just any
 * sample at all.  This is done by multiplying each prob with
 * state->n_samples/state->n_accepted.  Discard all but the first n_out
 * samples after adding them to the statistic.  If report is set, a
 * CSV-formatted report of the results is printed.  Additionally, some
 * statistical numbers are computed and printed out if verbose is set.
 */
static void
fix_up(FILE *outfile, FILE *prelimfile, struct samplestate *state,
    long long n_out, int report, int verbose)
{
	struct sample s;
	double size, adjust, variance = 0.0, p_1, paths, error;
	long long i;
	size_t count, samples_read = 0;

	if (state->n_accepted == 0) {
		fprintf(stderr, "no samples obtained\n");
		return;
	}

	if (verbose)
		fprintf(stderr, "fixing up %lld samples\n", state->n_accepted);

	adjust = (double)state->n_samples / state->n_accepted;
	size = state->size_sum / state->n_samples;

	rewind(prelimfile);

	i = 0;
	while (count = fread(&s, sizeof s, 1, prelimfile), count == 1) {
		samples_read++;

		s.p *= adjust;
		p_1 = 1.0 / s.p;
		variance += (size - p_1) * (size - p_1);

		if (i++ >= n_out)
			continue;

		count = fwrite(&s, sizeof s, 1, outfile);
		if (count != 1) {
			perror("fwrite");
			exit(EXIT_FAILURE);
		}
	}

	if (ferror(prelimfile)) {
		perror("fread");
		exit(EXIT_FAILURE);
	}

	assert(samples_read == state->n_accepted);

	variance /= state->n_accepted;
	paths = state->path_sum / (double)state->n_accepted;
	error = sqrt(variance / state->n_accepted);

	if (verbose) {
		fprintf(stderr, "size  %g\n", size);
		fprintf(stderr, "sdev  %g\n", sqrt(variance));
		fprintf(stderr, "moe68 %g (%5.2f%%)\n", 1 * error, 100 * error / size);
		fprintf(stderr, "moe95 %g (%5.2f%%)\n", 2 * error, 200 * error / size);
		fprintf(stderr, "moe99 %g (%5.2f%%)\n", 3 * error, 300 * error / size);
		fprintf(stderr, "paths %g\n", paths);
	}

	if (report) {
		printf("%d,%llu,%llu,%llu,%.16g,%.16g,%.16g,%.16g\n",
		    state->steps, state->n_samples, state->n_accepted, state->n_aborted,
		    size, sqrt(variance), error, paths);
	}
}

extern int
main(int argc, char *argv[])
{
	struct samplestate state;
	const struct fsm *fsm = &fsm_simple;
	struct pdb_catalogue *cat;
	FILE *fsmfile, *prelimfile, *outfile = NULL;
	long long n_puzzle = 1000, n_out = -1;
	int optchar, report = 0, verbose = 0, error;
	char *pdbdir = NULL;

	while (optchar = getopt(argc, argv, "d:j:m:n:N:o:rs:v"), optchar != -1)
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
			n_puzzle = strtol(optarg, NULL, 0);
			break;

		case 'N': /* number of samples to write into the sample file */
			n_out = strtol(optarg, NULL, 0);
			break;

		case 'o':
			outfile = fopen(optarg, "wb");
			if (outfile == NULL) {
				perror(optarg);
				return (EXIT_FAILURE);
			}

			break;

		case 'r':
			report = 1;
			break;

		case 's':
			set_seed(strtoll(optarg, NULL, 0));
			break;

		case 'v':
			verbose = 1;
			break;

		default:
			usage(argv[0]);
		}

	if (argc != optind + 2)
		usage(argv[0]);

	if (outfile == NULL) {
		fprintf(stderr, "Mandatory option -o outfile omitted\n");
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

	state.outfile = prelimfile;
	state.fsm = fsm;
	state.cat = cat;
	state.n_puzzle = n_puzzle;
	state.verbose = verbose;

	error = pthread_mutex_init(&state.lock, NULL);
	if (error != 0) {
		fprintf(stderr, "pthread_mutex_init: %s\n", strerror(error));
		return (EXIT_FAILURE);
	}

	state.n_samples = 0;
	state.n_accepted = 0;
	state.n_aborted = 0;
	state.path_sum = 0;
	state.size_sum = 0.0;
	state.steps = (int)strtol(argv[optind + 1], NULL, 0);
	if (state.steps < 0) {
		fprintf(stderr, "Number of steps cannot be negative: %s\n", argv[optind + 1]);
		usage(argv[0]);
	}

	take_samples_parallel(&state);

	if (n_out < 0)
		n_out = n_puzzle;
	fix_up(outfile, prelimfile, &state, n_out, report, verbose);

	return (EXIT_SUCCESS);
}
