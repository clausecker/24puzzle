/* random.c -- generate random puzzles and indices */
#include <string.h>

#include "puzzle.h"
#include "tileset.h"
#include "index.h"

/*
 * The seed used for the xorshift random number number generator.  This
 * variable is updated by all functions generating random objects.  Its
 * initial value has been drawn from /dev/random.
 */
unsigned random_seed = 0x70184fb2;

/*
 * Use the xorshift random number generator to generate a random number
 * between 0 and 2^32 - 1.
 */
static unsigned
xorshift(void)
{
	unsigned state = random_seed;

	state ^= state << 13;
	state &= 0xffffffff;
	state ^= state >> 17;
	state ^= state << 5;
	state &= 0xffffffff;

	random_seed = state;

	return (state);
}

/*
 * Set p to a random puzzle configuration drawn from random_seed.
 */
extern void
random_puzzle(struct puzzle *p)
{
	size_t i, j;

	memset(p, 0, sizeof *p);

	for (i = 0; i < TILE_COUNT; i++) {
		j = xorshift() % (i + 1);
		p->tiles[i] = p->tiles[j];
		p->tiles[j] = i;
	}

	for (i = 0; i < TILE_COUNT; i++)
		p->grid[p->tiles[i]] = i;
}

/*
 * Set i to a random index relative to aux.  This function also draws
 * its randomness from random_seed.
 */
extern void
random_index(const struct index_aux *aux, struct index *idx)
{
	tileset tsnz = tileset_remove(aux->ts, ZERO_TILE);

	idx->pidx = xorshift() % factorials[tileset_count(tsnz)];
	idx->maprank = xorshift() % combination_count[tileset_count(tsnz)];

	if (tileset_has(aux->ts, ZERO_TILE))
		idx->eqidx = xorshift() % aux->idxt[idx->maprank].n_eqclass;
	else
		idx->eqidx = -1;
}
