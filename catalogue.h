/*-
 * Copyright (c) 2017 Robert Clausecker. All rights reserved.
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

/* catalogue.h -- pattern database catalogues */

#ifndef CATALOGUE_H
#define CATALOGUE_H

#include <stdio.h>

#include "builtins.h"
#include "pdb.h"
#include "tileset.h"
#include "puzzle.h"
#include "heuristic.h"

/*
 * A struct pdb_catalogue stores a catalogue of pattern databases.
 * Groups of pattern databases are used to form distance heuristics, a
 * PDB catalogue stores multiple such groups and can compute the maximal
 * h value of all groups.  The member pdbs contains the pattern
 * databases we are interested in.  The member parts contains a bitmap
 * of which PDBs make up which heuristic.  The member heuristics
 * contains a bitmap of which heuristics each PDB is used for.  The
 * member pdbs_ts contains for the PDB's tile sets for better cache
 * locality.
 */
enum {
	CATALOGUE_HEUS_LEN = 64,
	HEURISTICS_LEN = 64,

	/* flags for catalogue_load() */
	CAT_IDENTIFY = 1 << 0,
};

struct pdb_catalogue {
	struct heuristic heus[CATALOGUE_HEUS_LEN];
	tileset pdbs_ts[CATALOGUE_HEUS_LEN];
	unsigned long long parts[HEURISTICS_LEN];
	size_t n_heus, n_heuristics;
};

/*
 * A struct partial_hvals stores the partial h values of a puzzle
 * configuration for the PDBs in a PDB catalogue.  This is useful so we
 * can avoid superfluous PDB lookups by not looking up values that did
 * not change change whenever we can.  The member fake_entries stores a
 * bitmap of those PDB whose entries we have not bothered to look up as
 * they do not contribute to the best heuristic for this puzzle
 * configuration.
 */
struct partial_hvals {
	unsigned char hvals[CATALOGUE_HEUS_LEN];
};

extern struct pdb_catalogue	*catalogue_load(const char *, const char *, int, FILE *);
extern void	catalogue_free(struct pdb_catalogue *);
extern int	catalogue_add_transpositions(struct pdb_catalogue *cat);
extern void	catalogue_partial_hvals(struct partial_hvals *, struct pdb_catalogue *, const struct puzzle *);
extern void	catalogue_diff_hvals(struct partial_hvals *, struct pdb_catalogue *, const struct puzzle *, unsigned);

/*
 * Given a struct partial_hvals, return the h value indicated
 * by this structure.  This is the maximum of all heuristics it
 * contains.
 */
static inline unsigned
catalogue_ph_hval(struct pdb_catalogue *cat, const struct partial_hvals *ph)
{
	size_t i;
	unsigned long long parts;
	unsigned max = 0, sum;

	for (i = 0; i < cat->n_heuristics; i++) {
		sum = 0;
		for (parts = cat->parts[i]; parts != 0; parts &= parts - 1)
			sum += ph->hvals[ctzll(parts)];

		if (sum > max)
			max = sum;
	}

	return (max);
}

/*
 * This convenience function call catalogue_partial_hvals() on a
 * throw-aray struct partial_hvals and just returns the result the
 * h value predicted for p by cat.
 */
static inline unsigned
catalogue_hval(struct pdb_catalogue *cat, const struct puzzle *p)
{
	struct partial_hvals ph;

	catalogue_partial_hvals(&ph, cat, p);

	return (catalogue_ph_hval(cat, &ph));
}

/*
 * Given a struct partial_hvals, return a bitmap indicating the
 * heuristics whose h value is equal to the maximum h value for
 * ph.
 */
static inline unsigned
catalogue_max_heuristics(struct pdb_catalogue *cat, const struct partial_hvals *ph)
{
	size_t i;
	unsigned long long parts;
	unsigned max = 0, sum, heumap = 0;

	for (i = 0; i < cat->n_heuristics; i++) {
		sum = 0;
		for (parts = cat->parts[i]; parts != 0; parts &= parts - 1)
			sum += ph->hvals[ctzll(parts)];

		if (sum > max) {
			max = sum;
			heumap = 0;
		}

		if (sum == max)
			heumap |= 1 << i;
	}

	return (heumap);
}

#endif /* CATALOGUE_H */
