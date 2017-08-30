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
 * tile set we use to compute indices.  The tables are organized first
 * by map rank, then by permutation index, and finally by equivalence
 * class.
 */
struct patterndb {
	struct index_aux aux;
	atomic_uchar *tables[];
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
extern int	pdb_store(FILE *, struct patterndb *);

/* various */
extern int	pdb_generate(struct patterndb *, FILE *);
extern int	pdb_verify(struct patterndb *, FILE *);
extern int	pdb_histogram(size_t[PDB_HISTOGRAM_LEN], struct patterndb *);
extern void	pdb_reduce(struct patterndb *, FILE *);

/*
 * Return a pointer to the PDB entry for idx.
 */
static inline atomic_uchar *
pdb_entry_pointer(struct patterndb *pdb, const struct index *idx)
{
	if (tileset_has(pdb->aux.ts, ZERO_TILE))
		return (pdb->tables[idx->maprank] + idx->eqidx * pdb->aux.n_perm + idx->pidx);
	else
		return (pdb->tables[idx->maprank] + idx->pidx);
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
 * Unconditionally update the PDB entry for idx to dist.
 */
static inline void
pdb_update(struct patterndb *pdb, const struct index *idx, unsigned dist)
{
	*pdb_entry_pointer(pdb, idx) = dist;
}

/*
 * Compare the PDB entry for idx with UNREACHED.  If it is equal, set it
 * to desired and return 1.  Otherwise, return 0.  This operation can be
 * executed concurrently with itself given that the value of desired is
 * equal in all calls.
 */
static inline int
pdb_conditional_update(struct patterndb *pdb, const struct index *idx, unsigned desired)
{
	atomic_uchar *entry = pdb_entry_pointer(pdb, idx);

	if (*entry != UNREACHED)
		return (0);

	return (atomic_exchange(entry, desired) == UNREACHED);
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


#endif /* PDB_H */
