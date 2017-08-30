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
