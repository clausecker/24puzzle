/*-
 * Copyright (c) 2017--2018 Robert Clausecker. All rights reserved.
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

/* random.c -- generate random puzzles and indices */
#include <string.h>
#include <pthread.h>

#include "puzzle.h"
#include "tileset.h"
#include "index.h"
#include "random.h"

/* the default random seed.  Taken from /dev/random. */
#define DEFAULT_SEED { \
	0xf8c53aa7, 0x16a4b97b, 0x13ed4568, 0x120e6496, \
	0x77bb4a8a, 0xeb39eae5, 0x46555774, 0x76d53591, \
	0x64f9b515, 0xc5185564, 0x76b545d0, 0xd02bebe1, \
	0xc73982f9, 0x5cc173a7, 0xb7002b87, 0x44d93488, \
	0xe42e0343, 0x19525a6c, 0x38005946, 0x3a92c714, \
	0x713da8b0, 0xad0d7988, 0x0788d23a, 0xd756c34c, \
	0x8d38a159, 0x47c83127, 0x65c0e1b3, 0x141c0dd6, \
	0xef0fea11, 0x4248804d, 0x19dd12ef, 0xe3c9b5da, \
}

/* the current state of the random number generator */
static unsigned int seed_v[32] = DEFAULT_SEED;
static unsigned char seed_i = 0;

/* a mutex guarding the seed */
pthread_mutex_t seed_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * Transition the random number generator by one step.  Return the
 * random number gained.  seed must be locked before calling this
 * function.  This function implements a WELL random number generator
 * as given by Wikipedia:
 *
 * https://de.wikipedia.org/w/index.php?title=Well_Equidistributed_Long-period_Linear&oldid=179828249
 */
static unsigned int
random_step(void)
{
	unsigned int z0, z1, z2, V0, VM1, VM2, VM3, VRm1;

	V0   = seed_v[seed_i];
	VM1  = seed_v[seed_i +  3 & 31];
	VM2  = seed_v[seed_i + 24 & 31];
	VM3  = seed_v[seed_i + 10 & 31];
	VRm1 = seed_v[seed_i + 31 & 31];

	z0 = VRm1;
	z1 = V0 ^ VM1 ^ VM1 >>  8; 
	z2 = (VM2 ^ VM2 << 19 ^ VM3 ^ VM3 << 14) & 0xffffffff;
	seed_v[seed_i] = z1 ^ z2;
	seed_v[seed_i + 31 & 31] = (z0 ^ z0 << 11 ^ z1 ^ z1 << 7 ^ z2 ^ z2 << 13) & 0xffffffff;
	seed_i = seed_i + 31 & 31;

	return (seed_v[seed_i]);
}

/*
 * Seed the random number generator with newseed.
 */
extern void
set_seed(unsigned long long newseed)
{
	static const unsigned int default_seed[32] = DEFAULT_SEED;
	int err, i;

	err = pthread_mutex_lock(&seed_lock);
	assert(err == 0);

	memcpy(seed_v, default_seed, 32 * sizeof *seed_v);
	seed_v[0] = newseed >>  0 & 0xffffffff;
	seed_v[1] = newseed >> 32 & 0xffffffff;
	seed_i = 0;

	/* mix the seed bytes into the rest of the state */
	for (i = 0; i < 1000; i++)
		random_step();

	err = pthread_mutex_unlock(&seed_lock);
	assert(err == 0);
}

/*
 * Compute a random 32 bit number.  This function is MT-safe.
 */
extern unsigned int
random32(void)
{
	unsigned int r;
	int err;

	err = pthread_mutex_lock(&seed_lock);
	assert(err == 0);

	r = random_step();

	err = pthread_mutex_unlock(&seed_lock);
	assert(err == 0);

	return (r);
}

/*
 * Compute a random 64 bit number.  This function is MT-safe.
 */
extern unsigned long long
random64(void)
{
	unsigned int r1, r2;
	int err;

	err = pthread_mutex_lock(&seed_lock);
	assert(err == 0);

	r1 = random_step();
	r2 = random_step();

	err = pthread_mutex_unlock(&seed_lock);
	assert(err == 0);

	return ((unsigned long long)r1 << 32 | r2);
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
	__uint128_t rnd;
	unsigned long long rnd1, rnd2;
	size_t i, j, parity = 0;
	int ipos, jpos, err;

	/* silence valgrind as we technically read uninitialized values */
	memset(p, 0, sizeof *p);

	err = pthread_mutex_lock(&seed_lock);
	assert(err == 0);

	/*
	 * Since we consume many of the 128 bits of entropy we get, we
	 * must be careful to avoid modulo bias.  This is done by
	 * capping the range of random numbers we admit upwards to get a
	 * clean multiple of the range we are interested in.
	 */
	do {
		rnd = 0;
		for (i = 0; i < 4; i++)
			rnd = rnd << 32 | random_step();

		/* 23 * 24 * 25! */
	} while (rnd >= (__uint128_t)39742454749 * 23 * 24 *
		    2432902008176640000ULL * 6375600ULL /* 25! */);

	err = pthread_mutex_unlock(&seed_lock);
	assert(err == 0);

	rnd1 = rnd % 2432902008176640000ULL; /* 1 * 2 * ... * 20 */
	rnd2 = rnd / 2432902008176640000ULL;

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
	unsigned long long rnd = random64();
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

/*
 * Perform an n step random walk from p, using fsm to prune moves.
 * Return 1 if the random walk was successful, 0 otherwise.  A random
 * walk is unsuccessful if the fsm at some point doesn't provide us
 * with a move to progress.
 */
extern int
random_walk(struct puzzle *p, int steps, const struct fsm *fsm)
{
	struct fsm_state st;
	unsigned int entropy;
	int err, res = 0, i, n_move, reservoir = 0;
	signed char moves[4];

	err = pthread_mutex_lock(&seed_lock);
	assert(err == 0);

	entropy = random_step();
	reservoir = 16;

	st = fsm_start_state(zero_location(p));

	while (steps > 0) {
		n_move = fsm_get_moves(moves, st, fsm);

		switch (n_move) {
		case 0:	goto fail; /* cannot proceed */

		case 1: i = 0; /* no choice to make */
			break;

		default:
			do {
				if (reservoir == 0) {
					entropy = random_step();
					reservoir = 16;
				}

				i = entropy & 3;
				entropy >>= 2;
				reservoir--;
			} while (i >= n_move);
		}

		st = fsm_advance(fsm, st, moves[i]);
		move(p, moves[i]);
		steps--;
	}

	res = 1;

fail:	err = pthread_mutex_lock(&seed_lock);
	assert(err == 0);

	return (res);
}
