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
atomic_ullong random_seed = 0x70184fb2;

/*
 * Use the xorshift random number generator to generate a random number
 * between 0 and 2^32 - 1.  We use an atomic exchange operation to make
 * sure that each result of the RNG is consumed exactly once.
 */
static unsigned long long
xorshift(void)
{
	unsigned long long seed = random_seed, state;

	do {
		state = seed;
		state ^= state >> 13;
		state ^= state << 7;
		state &= 0xffffffffffffffffull;
		state ^= state >> 17;
	} while (!atomic_compare_exchange_weak(&random_seed, &seed, state));

	return (state);
}

/*
 * Set p to a random puzzle configuration drawn from random_seed.
 */
extern void
random_puzzle(struct puzzle *p)
{
	unsigned long long rnd = xorshift();
	size_t i, j;

	memset(p, 0, sizeof *p);

	for (i = 0; i < TILE_COUNT; i++) {
		j = rnd % (i + 1);
		rnd /= (i + 1);
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
	unsigned long long rnd = xorshift();
	tileset tsnz = tileset_remove(aux->ts, ZERO_TILE);

	idx->pidx = rnd % factorials[tileset_count(tsnz)];
	rnd /= factorials[tileset_count(tsnz)];
	idx->maprank = xorshift() % combination_count[tileset_count(tsnz)];

	if (tileset_has(aux->ts, ZERO_TILE))
		idx->eqidx = xorshift() % aux->idxt[idx->maprank].n_eqclass;
	else
		idx->eqidx = -1;
}
