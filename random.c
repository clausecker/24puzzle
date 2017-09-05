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
 * A 64 bit xorshift step function with parameters (13, 7, 17).
 */
static unsigned long long
xorshift_step(unsigned long long x)
{
	x ^= x >> 13;
	x ^= x << 7;
	x &= 0xffffffffffffffffull;
	x ^= x >> 17;

	return (x);
}

/*
 * Use the xorshift random number generator to generate a random number
 * between 0 and 2^64 - 1.  We use an atomic exchange operation to make
 * sure that each result of the RNG is consumed exactly once.
 */
static unsigned long long
xorshift(void)
{
	unsigned long long seed = random_seed, state;

	do state = xorshift_step(seed);
	while (!atomic_compare_exchange_weak(&random_seed, &seed, state))
		;

	return (state);
}

/*
 * Set p to a random puzzle configuration drawn from random_seed.  Note
 * that since a puzzle configuration has 82.7 bits of entropy but we
 * only extract 64 bits of entry from the RNG, not all puzzle
 * configurations are guaranteed to be generated.
 */
extern void
random_puzzle(struct puzzle *p)
{
	unsigned long long rnd1 = xorshift(), rnd2 = xorshift_step(rnd1);
	size_t i, j, zloc, parity = 0;
	int ipos, jpos;

	/* silence valgrind as we technically read uninitialized values */
	memset(p, 0, sizeof *p);

	/* consumes log2(1 * 2 * 3 * ... * 20) = 61.0774 bits of entropy */
	for (i = 0; i < 20; i++) {
		j = rnd1 % (i + 1);
		rnd1 /= i + 1;
		p->tiles[i] = p->tiles[j];
		p->tiles[j] = i;
		parity ^= i != j;
	}

	/* consumes log2(21 * ... * 25) = 22.6041 bits of entropy */
	for (i = 20; i < 25; i++) {
		j = rnd2 % (i + 1);
		rnd2 /= i + 1;
		p->tiles[i] = p->tiles[j];
		p->tiles[j] = i;
		parity ^= i != j;
	}

	/*
	 * For the puzzle to be solvable, the the parity of the tile
	 * permutation must agree with the parity of the zero tile's
	 * square number.  If this parity is violated, we simply swap
	 * two random tiles to restore it.  If this needs to be done,
	 * log2(24 * 23) = 9.10852 bits of entropy are consumed.
	 */
	if (parity ^ zero_location(p) & 1) {
		i = 1 + rnd2 % 24;
		rnd2 /= 24;
		j = 1 + rnd2 % 23;
		j += j >= i;

		ipos = p->tiles[i];
		jpos = p->tiles[j];

		p->tiles[i] = jpos;
		p->tiles[j] = ipos;
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
	idx->maprank = rnd % combination_count[tileset_count(tsnz)];
	rnd /= combination_count[tileset_count(tsnz)];

	if (tileset_has(aux->ts, ZERO_TILE))
		idx->eqidx = rnd % aux->idxt[idx->maprank].n_eqclass;
	else
		idx->eqidx = -1;
}
