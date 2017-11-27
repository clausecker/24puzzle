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

#ifndef BITPDB_H
#define BITPDB_H

#include <limits.h>
#include <stdio.h>

#include "index.h"
#include "tileset.h"
#include "puzzle.h"
#include "pdb.h"

/*
 * For the sake of IDA*, we only need to know if a move is predicted as
 * getting closer to the goal or farther away.  If the PDB represents a
 * consistent heuristic, this means that we only need to care about the
 * least two bits of the h value.  The following transisitions are
 * possible in a bipartite graph and can all be detected by knowing just
 * the least two signficant bits from the previous and the current
 * configuration.
 *
 *     00 -> 01    sad          10 -> 11    sad
 *     00 -> 11    happy        10 -> 01    happy
 *     01 -> 10    sad          11 -> 00    sad
 *     01 -> 00    happy        11 -> 10    happy
 *
 * when computing the h value for a configuration, we can find the least
 * significant bit by observing the configuration's parity.  Thus, only
 * the second to least significant bit has to be stored in the PDB.  The
 * other bits can be reconstructed if needed by following happy moves in
 * the PDB's quotient graph until the goal is reached.
 *
 * Note that this technique probably doesn't work with identified PDBs
 * due to their inconsistency.  Using an identified PDB as a bitpdb is
 * undefined behaviour.



 *
 * struct bitpdb represents such a bitpdb.  The layout is similar to the
 * layout of a normal PDB, but looking up a position directly is slower.
 * For best performance, only differential lookups should be performed.
 */
struct bitpdb {
	struct index_aux aux;
	unsigned char *data;
};

/* bitpdb.c */
extern struct bitpdb	*bitpdb_allocate(tileset);
extern void		 bitpdb_free(struct bitpdb *);
extern struct bitpdb	*bitpdb_load(tileset, FILE *);
// extern struct bitpdb	*bitpdb_mmap(tileset, int, int);
extern int		 bitpdb_store(FILE *, struct bitpdb *);
extern int		 bitpdb_lookup_puzzle(struct bitpdb *, const struct puzzle *);
extern int		 bitpdb_diff_lookup(struct bitpdb *, const struct puzzle *, int);

/* bitpdbzstd.c */
extern struct bitpdb	*bitpdb_load_compressed(tileset, FILE *);
extern int		 bitpdb_store_compressed(FILE *, struct bitpdb *);

/* bitreduce.c */
extern struct bitpdb	*bitpdb_from_pdb(struct patterndb *);

/*
 * Return the size of the data table for a bitpdb corresponding to aux.
 */
static inline
size_t bitpdb_size(struct index_aux *aux)
{
	return ((search_space_size(aux) + CHAR_BIT - 1) / CHAR_BIT);
}

#endif /* BITPDB_H */
