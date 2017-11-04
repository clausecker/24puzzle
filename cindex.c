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

/* cindex.c -- combined indices */

#include "index.h"

/*
 * The maximal number of equivalence classes.
 */
enum { EQCLASS_MAX = 8, };

/*
 * Combine a struct index into a single scalar.  This is useful if we
 * need to store many indices at the same time.  Note that the combined
 * indices are not necessarily continuous.
 */
extern cindex
combine_index(const struct index_aux *aux, const struct index *idx)
{
	cindex c = idx->maprank * aux->n_perm + idx->pidx;

	if (tileset_has(aux->ts, ZERO_TILE))
		return (c * EQCLASS_MAX + idx->eqidx);
	else
		return (c);
}

/*
 * Convert a combined index back into a normal index.
 */
extern void
split_index(const struct index_aux *aux, struct index *idx, cindex c)
{

	idx->eqidx = -1;

	if (tileset_has(aux->ts, ZERO_TILE)) {
		idx->eqidx = c % EQCLASS_MAX;
		c /= EQCLASS_MAX;
	}

	idx->pidx = c % aux->n_perm;
	idx->maprank = c / aux->n_perm;
}
