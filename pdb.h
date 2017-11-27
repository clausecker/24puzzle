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

#ifndef PDB_H
#define PDB_H

#include <stdatomic.h>
#include <stdio.h>
#include <limits.h>

#include "tileset.h"
#include "index.h"

/*
 * A patterndb is an array of pointers to arrays of bytes
 * representing the distance from the represented partial puzzle
 * configuration to the solved puzzle.  The member aux describes the
 * tile set we use to compute indices.  data points to the content of
 * the PDB, organized first by map rank, then by permutation index,
 * and finally by equivalence class.
 */
struct patterndb {
	struct index_aux aux;
	int mapped; /* true if PDB has been allocated using mmap() */
	atomic_uchar *data;
};

_Static_assert(sizeof(atomic_uchar) == 1, "Machine does not support proper atomic chars.");

/*
 * A value representing an infinite distance to the solved position,
 * i.e. a PDB entry that hasn't been filled in yet.
 */
enum { UNREACHED = (unsigned char)-1 };

enum {
	/* max number of jobs allowed */
	PDB_MAX_JOBS = 256,

	/* maximum number of entries in a PDB histogram */
	PDB_HISTOGRAM_LEN = 256,

	/* constants for pdb_mmap */
	PDB_MAP_RDONLY = 0,
	PDB_MAP_RDWR = 1,
	PDB_MAP_SHARED = 2,

	/* the maximal amount of PDBs used at once */
	PDB_MAX_COUNT = TILE_COUNT - 1,
};

/*
 * The number of threads to use.  This must be between 1 and
 * PDB_MAX_JOBS and is set to 1 initially.  This is a global variable
 * intended to be set once during program initialization.  Since its
 * value typically does not change during operation, the author deemed
 * it more useful to have this be a global variable instead of passing
 * it around everywhere.
 */
extern int pdb_jobs;

/* pdb.c */
extern struct patterndb	*pdb_allocate(tileset);
extern void	pdb_free(struct patterndb *);
extern void	pdb_clear(struct patterndb *);
extern struct patterndb *pdb_load(tileset, FILE *);
extern struct patterndb *pdb_mmap(tileset, int, int);
extern int	pdb_store(FILE *, struct patterndb *);

/* various */
extern int	pdb_generate(struct patterndb *, FILE *);
extern int	pdb_verify(struct patterndb *, FILE *);
extern int	pdb_histogram(size_t[PDB_HISTOGRAM_LEN], struct patterndb *);
extern void	pdb_identify(struct patterndb *);
extern void	pdb_diffcode(struct patterndb *, unsigned char[]);

/*
 * Return a pointer to the PDB entry for idx.
 */
static inline atomic_uchar *
pdb_entry_pointer(struct patterndb *pdb, const struct index *idx)
{

	return (pdb->data + index_offset(&pdb->aux, idx));
}

/*
 * Look up the distance of the partial configuration represented by idx
 * in the pattern database and return it.
 */
static inline int
pdb_lookup(struct patterndb *pdb, const struct index *idx)
{
	return (*pdb_entry_pointer(pdb, idx));
}

/*
 * Prefetch the PDB entry for idx.
 */
static inline void
pdb_prefetch(struct patterndb *pdb, const struct index *idx)
{
	prefetch(pdb_entry_pointer(pdb, idx));
}

/*
 * Unconditionally update the PDB entry for idx to dist.  The update is
 * performed with memory_order_relaxed.
 */
static inline void
pdb_update(struct patterndb *pdb, const struct index *idx, unsigned dist)
{
	atomic_store_explicit(pdb_entry_pointer(pdb, idx), dist, memory_order_relaxed);
}

/*
 * Update the PDB entry idx to desired if it is equal to UNREACHED.
 * This is done with memory_order_relaxed.
 */
static inline void
pdb_conditional_update(struct patterndb *pdb, const struct index *idx, unsigned desired)
{
	atomic_uchar *entry = pdb_entry_pointer(pdb, idx);

	if (atomic_load_explicit(entry, memory_order_relaxed) == UNREACHED)
		atomic_store_explicit(entry, desired, memory_order_relaxed);
}

/*
 * Return the number entries in table i in pdb.
 */
static inline size_t
pdb_table_size(struct patterndb *pdb, size_t i)
{

	return (pdb->aux.n_perm * (tileset_has(pdb->aux.ts, ZERO_TILE) ?
	    pdb->aux.idxt[i].n_eqclass : 1));
}

/*
 * Lookup puzzle configuration p in the pattern database pdb and return
 * the distance found.  This is a convenience function so callers do not
 * have to deal with indices manually.
 */
static inline int
pdb_lookup_puzzle(struct patterndb *pdb, const struct puzzle *p)
{
	struct index idx;

	compute_index(&pdb->aux, &idx, p);
	return (pdb_lookup(pdb, &idx));
}

#endif /* PDB_H */
