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

#ifndef HEURISTIC_H
#define HEURISTIC_H

#include <stdio.h>

#include "tileset.h"
#include "puzzle.h"
#include "transposition.h"

/*
 * A heuristic provides h values for a given tile set.  Heuristics for
 * different tile sets can be added and remain admissible.  This
 * structure is an abstraction over different heuristic providers,
 * currently normal PDBs and bitpdbs.  The heuristic abstraction also
 * provides logic to abstract away search on transposed configurations.
 * The underlying heuristic provider is queried using the hval function
 * provider.  A call to the free function pointer should release the
 * storage associated with the underlying heuristic.
 */
struct heuristic {
	void *provider;
	int (*hval)(void *, const struct puzzle *);
	void (*free)(void *);
	tileset ts;
	unsigned morphism; /* the automorphism to apply */
};

/* flags for heu_open() */
enum {
	HEU_CREATE =  1 << 0,   /* create heuristic if not found */
	HEU_NOMORPH = 1 << 1,   /* do not try to apply morphisms */
	HEU_VERBOSE = 1 << 2,   /* print status messages to stderr */
	HEU_SIMILAR = 1 << 3,   /* try to find a similar PDB, too */
};

/*
 * the following types are supported:
 *
 * pdb     additive pattern database
 * zpdb    zero-aware pattern database
 * bitpdb  additive bit pattern database
 * zbitpdb zero-aware bit pattern database
 *
 * the type can be suffixed with ".zst" to make heu_open generate a
 * zstd compressed pattern database.
 */

extern int	heu_open(struct heuristic *, const char *, tileset, const char *, int);

/*
 * Look up the h value provided by heu for p.
 */
static inline unsigned
heu_hval(struct heuristic *heu, const struct puzzle *p)
{
	struct puzzle p_morphed;
	const struct puzzle *pp = p;

	if (heu->morphism != 0) {
		p_morphed = *p;
		morph(&p_morphed, heu->morphism);
		pp = &p_morphed;
	}

	return (heu->hval(heu->provider, pp));
}

/*
 * Release the storage associated with heu.
 */
static inline void
heu_free(struct heuristic *heu)
{
	heu->free(heu->provider);
}

/*
 * Derive a heuristic by applying the given morphism to an existing
 * heuristic.  Note that the two heuristics share an underlying
 * heuristic provider, calling heu_free() on one of them also frees
 * the other one!
 */
static inline void
heu_morph(struct heuristic *heu, struct heuristic *oldheu, int morphism)
{
	heu->provider = oldheu->provider;
	heu->hval = oldheu->hval;
	heu->free = oldheu->free;
	heu->ts = tileset_morph(oldheu->ts, morphism);
	heu->morphism = compose_morphisms(oldheu->morphism, morphism);
}


#endif /* HEURISTIC_H */
