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

/* sampleeta.c -- estimate eta by stratified sample */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <math.h>

#include "catalogue.h"
#include "statistics.h"
#include "random.h"
#include "search.h"
#include "fsm.h"

/* flags for output control */
enum {
	VERBOSE = 1 << 0,
	VERIFY = 1 << 1,
};

struct stratum {
	long long n_samples;	/* actual number of samples */
	double eta;		/* eta value determined for the stratum */
	double var;		/* variance determined for the stratum */
	double size;		/* stratum size as a fraction of 1 */
};

/*
 * Compute b^-h(v) for a given puzzle configuration and a given heuristic.
 */
static double
pow_h(const struct puzzle *p, struct pdb_catalogue *cat)
{
	return (pow(B, -(double)catalogue_hval(cat, p)));
}

/*
 * Taking samples from filename, sample a stratum using up to
 * max_samples samples.  Less are used if the sample file has less
 * entries.  If verbose is set, print a statistical report to
 * stderr.  Save the results of the sample to str.  Return -1 on
 * error, 0 on success.
 */
static int
sample_sphere(int d, struct stratum *str, struct pdb_catalogue *cat, FILE *samplefile,
    long long max_samples, int verbose)
{
	struct puzzle p;
	struct sample s;
	long long i;
	size_t count;
	double diff, accum = 0.0;

	/* first pass: compute expected value */
	i = 0;
	while (i < max_samples &&
	    (count = fread(&s, sizeof s, 1, samplefile), count == 1)) {
		i++;
		unpack_puzzle(&p, &s.cp);
		accum += pow_h(&p, cat) / s.p;
	}

	if (ferror(samplefile))
		return (-1);

	str->n_samples = i;
	str->eta = accum / (sphere_sizes[d] * str->n_samples);

	/* second pass: compute variance */
	rewind(samplefile);
	accum = 0.0;
	i = 0;
	while (i < str->n_samples &&
	    (count = fread(&s, sizeof s, 1, samplefile), count == 1)) {
		i++;
		unpack_puzzle(&p, &s.cp);
		diff = str->eta - pow_h(&p, cat);
		accum += diff * diff / (sphere_sizes[d] * s.p);
	}

	if (ferror(samplefile))
		return (-1);

	str->var = accum / str->n_samples;
	str->size = sphere_sizes[d] / CONFCOUNT;

	if (verbose)
		fprintf(stderr, "%4d avg %#e part %#e sdev %#e samples %8lld\n",
		    d, str->eta, str->eta * str->size, sqrt(str->var), str->n_samples);

	return (0);
}

/*
 * Size of the search space minus the spheres up to and including limit.
 */
static double
rest_size(int limit)
{
	double accum = 0.0;
	int i;

	for (i = 0; i <= limit; i++)
		accum += sphere_sizes[i];

	return (CONFCOUNT - accum);
}

/*
 * generate n_samples random samples from the search space.  If VERIFY
 * is set in flags, make sure they have a distance of more than lower.
 * If VERBOSE is set in flags, print a human-readable record to stderr.
 * Return 0 on success, -1 on error.  Store statistical data to str.
 */
static int
sample_rest(int lower, struct stratum *str, struct pdb_catalogue *cat,
    long long rest_samples, int flags)
{
	struct puzzle p;
	long long i = 0, rejects = 0;
	double accum, obs;
	unsigned char *hvals;

	assert(rest_samples >= 0);
	hvals = malloc(rest_samples * sizeof *hvals);
	if (hvals == NULL)
		return (-1);

	/* take samples */
	while (i < rest_samples) {
		struct path pa;

		random_puzzle(&p);

		if (flags & VERIFY) {
			search_ida_bounded(cat, &fsm_simple, &p, lower, &pa, NULL, NULL, 0);
			if (pa.pathlen != SEARCH_NO_PATH) {
				rejects++;
				continue;
			}
		}

		hvals[i++] = catalogue_hval(cat, &p);
	}

	str->n_samples = i;

	/* compute arithmetic mean */
	accum = 0.0;
	for (i = 0; i < rest_samples; i++)
		accum += pow(B, -(double)hvals[i]);

	str->eta = accum / str->n_samples;

	/* compute variance */
	accum = 0.0;
	for (i = 0; i < rest_samples; i++) {
		obs = str->eta - pow(B, -(double)hvals[i]);
		accum += obs * obs;
	}

	str->var = accum / str->n_samples;
	str->size = rest_size(lower) / CONFCOUNT;
	free(hvals);

	if (flags & VERBOSE)
		fprintf(stderr, "rest avg %#e part %#e sdev %#e samples %8lld rej %lld)\n",
		    str->eta, str->eta * str->size, sqrt(str->var),
		    str->n_samples, rejects);

	return (0);
}

/*
 * Compute eta from the various strata's partial results.
 * Also compute variance, standard deviation, and confidence
 * intervals.
 */
static void
join_strata(struct stratum *strata, int limit)
{
	double eta = 0.0, var = 0.0, error = 0.0, diff, pop_corr, comp;
	int i;

	/* compute eta */
	for (i = 0; i <= limit + 1; i++)
		eta += strata[i].eta * strata[i].size;

	/* compute standard deviation */
	for (i = 0; i <= limit + 1; i++) {
		diff = eta - strata[i].eta;
		var += (strata[i].var + diff * diff) * strata[i].size;
	}

	/* compute standard error */
	/* exclude i = 0 to avoid division by zero */
	for (i = 1; i <= limit + 1; i++) {
		pop_corr = (strata[i].size * CONFCOUNT - strata[i].n_samples)
		    / (strata[i].size * CONFCOUNT - 1);
		comp = strata[i].size * strata[i].size * strata[i].var
		    / strata[i].n_samples;
		error += pop_corr * comp;
	}

	printf("eta   %#e\nsdev  %#e\nerror %#e\n", eta, sqrt(var), sqrt(error));
}

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-vV] [-j nproc] [-s seed] [-n max_samples]"
	    " [-d pdbdir] [-l limit] [-p sample_prefix] [-r rest_samples]"
	    " catalogue\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	struct pdb_catalogue *cat;
	struct stratum *strata;
	FILE *samplefile;
	long long max_samples = LLONG_MAX, rest_samples = 1000000;
	int i, optchar, flags = 0, limit = MAX_SPHERE;
	char *prefix = NULL, *pdbdir = NULL, filename[PATH_MAX];

	while (optchar = getopt(argc, argv, "d:j:l:n:p:r:s:v:V:"), optchar != -1)
		switch (optchar) {
		case 'd':
			pdbdir = optarg;
			break;

		case 'j':
			pdb_jobs = atoi(optarg);
			if (pdb_jobs < 1 || pdb_jobs > PDB_MAX_JOBS) {
				fprintf(stderr, "Number of threads must be between 1 and %d: %s\n",
				    PDB_MAX_JOBS, optarg);
				return (EXIT_FAILURE);
			}

			break;

		case 'l':
			limit = atoi(optarg);
			break;

		case 'n':
			max_samples = atoll(optarg);
			if (max_samples <= 0) {
				fprintf(stderr, "Sample count limit must be positive: %s\n", optarg);
				exit(EXIT_FAILURE);
			}

			break;

		case 'p':
			prefix = optarg;
			break;

		case 'r':
			rest_samples = atoll(optarg);
			if (rest_samples < 0) {
				fprintf(stderr, "Sample count for graph rest may not be negative: %s\n", optarg);
				exit(EXIT_FAILURE);
			}

			break;

		case 's':
			set_seed(strtoll(optarg, NULL, 0));
			break;

		case 'v':
			flags |= VERBOSE;
			break;

			/*
			 * Verify that the samples picked from the last
			 * stratum (i.e. the rest of the graph) do not
			 * accidentally lie in one of the earlier strata.
			 */
		case 'V':
			flags |= VERIFY;
			break;

		default:
			usage(argv[0]);
			break;
		}

	/* don't use more spheres than we know the size of */
	if (limit < -1 || limit > MAX_SPHERE)
		limit = MAX_SPHERE;

	/* don't attempt to read sample files if no prefix given */
	if (prefix == NULL)
		limit = -1;

	if (argc != optind + 1)
		usage(argv[0]);

	cat = catalogue_load(argv[optind], pdbdir, 0, flags & VERBOSE ? stderr : NULL);
	if (cat == NULL) {
		perror("catalogue_load");
		return (EXIT_FAILURE);
	}

	/* limit+1 strata for the spheres, one for the rest of the graph */
	strata = calloc(sizeof *strata, limit + 2);
	if (strata == NULL) {
		perror("calloc");
		return (EXIT_FAILURE);
	}

	for (i = 0; i <= limit; i++) {
		snprintf(filename, sizeof filename, "%s%d.sample", prefix, i);
		samplefile = fopen(filename, "rb");
		if (samplefile == NULL)
			/* if the sample file doesn't exist, assume we ran out of spheres */
			if (errno == ENOENT) {
				limit = i - 1;
				break;
			} else {
				perror(filename);
				return (EXIT_FAILURE);
			}

		if (sample_sphere(i, strata + i, cat, samplefile, max_samples, flags & VERBOSE) != 0) {
			perror("sample_sphere");
			return (EXIT_FAILURE);
		}

		fclose(samplefile);
	}

	if (sample_rest(limit, strata + limit + 1, cat, rest_samples, flags) != 0) {
		perror("sample_rest");
		return (EXIT_FAILURE);
	}

	join_strata(strata, limit);

	return (EXIT_SUCCESS);
}
