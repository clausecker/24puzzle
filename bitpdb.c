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

/* bitpdb.c -- functionality to deal with bitpdbs */

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "bitpdb.h"
#include "index.h"
#include "puzzle.h"
#include "tileset.h"

/*
 * Allocate a bitpdb for tile set ts.  If storage is insufficient,
 * return NULL and set errno.  The entries in the bitpdb are undefined
 * initially.  This function isn't really useful for anything other
 * than internal functionality.
 */
extern struct bitpdb *
bitpdb_allocate(tileset ts)
{
	struct bitpdb *bpdb;
	int error;

	bpdb = malloc(sizeof *bpdb);
	if (bpdb == NULL)
		return (NULL);

	make_index_aux(&bpdb->aux, ts);
	bpdb->data = malloc(bitpdb_size(&bpdb->aux));
	if (bpdb->data == NULL) {
		error = errno;
		free(bpdb);
		errno = error;
		return (NULL);
	}

	return (bpdb);
}

/*
 * Release storage associated with bpdb.
 */
extern void
bitpdb_free(struct bitpdb *bpdb)
{

	free(bpdb->data);
	free(bpdb);
}

/*
 * Load a bitpdb for tileset ts from FILE f and return a pointer to the
 * bitpdb just loaded.  On error, return NULL and set errno to indicate
 * the problem.  pdbfile must be a binary file opened for reading with
 * the file pointer positioned right at the beginning of the bitpdb.
 * The file pointer is located at the end of the bitpdb on success and
 * is undefined on failure.
 */
extern struct bitpdb *
bitpdb_load(tileset ts, FILE *pdbfile)
{
	struct bitpdb *bpdb = bitpdb_allocate(ts);
	size_t count, size;
	int error;

	if (bpdb == NULL)
		return (NULL);

	size = bitpdb_size(&bpdb->aux);
	count = fread((void *)bpdb->data, 1, size, pdbfile);
	if (count != size) {
		error = errno;
		bitpdb_free(bpdb);

		/* tell apart short read from IO error */
		if (!ferror(pdbfile))
			errno = EINVAL;

		else
			errno = error;

		return (NULL);
	}

	return (bpdb);
}

/*
 * Write bpdb to FILE f.  Return 0 on success, -1 on error.  Set errno
 * to indicate the cause on error. f must be a binary file open for
 * writing, the file pointer is positioned after the end of the bitpdb
 * on success, undefined on failure.
 */
extern int
bitpdb_store(FILE *f, struct bitpdb *bpdb)
{
	size_t count, size = bitpdb_size(&bpdb->aux);
	int error;

	count = fwrite((void *)bpdb->data, 1, size, f);
	if (count != size) {
		error = errno;

		if (!ferror(f))
			errno = ENOSPC;
		else
			errno = error;

		return (-1);
	}

	fflush(f);

	return (0);
}

/*
 * Return the parity of the partial puzzle configuration whose index
 * is idx with respect to aux which is the parity of the set of grid
 * locations occupied in p by the tiles in aux.
 */
static int
partial_parity(const struct index_aux *aux, const struct puzzle *p)
{
	return (tileset_parity(tile_map(aux, p)) ^ aux->solved_parity);
}

/*
 * Lookup the entry bit for puzzle configuration p in bpdb.  Return the
 * bit shifted to the second least significant bit.
 */
static int
bpdb_lookup_bit(struct bitpdb *bpdb, const struct index *idx)
{
	size_t offset;
	int entry;

	offset = index_offset(&bpdb->aux, idx);

	entry = bpdb->data[offset / CHAR_BIT] >> (offset % CHAR_BIT) & 1;

	return (entry << 1);
}

/*
 * Perform a differential lookup into bpdb.  old_h must be the h value
 * for a puzzle configuration directly connected with p but not
 * identical to p in the quotient graph induced by bpdb->aux.ts.  idx must
 * be the index for p.  Return the distance found.
 */
static int
bitpdb_diff_lookup_idx(struct bitpdb *bpdb, const struct puzzle *p,
    int old_h, const struct index *idx)
{
	int entry = bpdb_lookup_bit(bpdb, idx);

	/* assure p has different parity than old_h as a sanity check */
	(void)partial_parity;
	assert((partial_parity(&bpdb->aux, p) ^ old_h & 1) == 1);

	/* increment old_h if (entry ^ old_h ^ old_h << 1) & 2 else decrement */
	return (old_h - 1 + ((entry ^ old_h ^ old_h << 1) & 2));
}

/*
 * Like bitpdb_diff_lookup, but compute the index, too.
 */
extern int
bitpdb_diff_lookup(struct bitpdb *bpdb, const struct puzzle *p, int old_h)
{
	struct index idx;

	compute_index(&bpdb->aux, &idx, p);

	return (bitpdb_diff_lookup_idx(bpdb, p, old_h, &idx));
}

/*
 * Determine the h value for puzzle configuration p by looking up a
 * shortest path in the quotient graph induced by bpdb->aux.ts in bpdb.
 * This operation is rather slow and should only be used to get an
 * initial h value, for further search, bitpdb_diff_lookup() should be
 * used instead.
 */
extern int
bitpdb_lookup_puzzle(struct bitpdb *bpdb, const struct puzzle *parg)
{
	struct move moves[MAX_MOVES];
	struct index idx;
	struct puzzle p = *parg;
	size_t n_moves, i;
	int initial_h, cur_h, next_h;

	/* some even value higher than the diameter of the search space */
	enum { DUMMY_HVAL = 250 };

	cur_h = initial_h = DUMMY_HVAL | partial_parity(&bpdb->aux, &p);
	compute_index(&bpdb->aux, &idx, &p);

	while (!puzzle_partially_equal(&p, &solved_puzzle, &bpdb->aux)) {
		n_moves = generate_moves(moves, eqclass_from_index(&bpdb->aux, &idx));

		assert(n_moves > 0);
		for (i = 0; i < n_moves; i++) {
			move(&p, moves[i].zloc);
			move(&p, moves[i].dest);

			compute_index(&bpdb->aux, &idx, &p);
			next_h = bitpdb_diff_lookup_idx(bpdb, &p, cur_h, &idx);
			assert(abs(next_h - cur_h) == 1);
			if (next_h < cur_h)
				break;

			move(&p, moves[i].zloc);
		}

		/* make sure we made progess */
		assert(next_h < cur_h);
		cur_h = next_h;
	}

	return (initial_h - cur_h);
}
