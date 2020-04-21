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

/* statistics.h -- utilies to deals with statistics */

#ifndef STATISTICS_H
#define STATISTICS_H

#include <stdio.h>

#include "pdb.h"
#include "puzzle.h"
#include "compact.h"

/*
 * The content of a stat file.  These files first contain
 * total number of samples and then lines of the form
 *
 *     AA: BBBB/ CCCC = D.DDDe-DD
 *
 * where A is a distance, B is the number of samples found for this
 * distance, C is the total number of samples and D.DDDe-DD is B/C in
 * scientific notation.  The max_i member is the largest index present
 * in the statistics file.
 */
struct stat_file {
	double hits[PDB_HISTOGRAM_LEN];
	double samples[PDB_HISTOGRAM_LEN];
	double total;
	int max_i;
};

/* highest sphere for which we know the size */
enum { MAX_SPHERE = 55, };

extern int	parse_stat_file(struct stat_file *, FILE *);
extern void	write_stat_file(FILE *, const struct stat_file *);

extern const double equilibrium_bias[TILE_COUNT];
extern const double sphere_sizes[MAX_SPHERE + 1];

/*
 * Compute the equilibrium bias of a puzzle p.
 */
static inline double
bias_of(const struct puzzle *p)
{
	return (equilibrium_bias[zero_location(p)]);
}

/*
 * a sphere-stratified sample as written to a sample file.  Each sample
 * contains both a puzzle description and the probability of having
 * reached it through whatever sampling process was used to find the
 * sample.
 */
struct sample {
	struct compact_puzzle cp;
	double p;
};

#endif /* STATISTICS_H */
