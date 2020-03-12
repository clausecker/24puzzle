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

#include "puzzle.h"
#include "tileset.h"
#include "index.h"
#include "random.h"
/*
 * The seed used for the xorshift random number number generator.  This
 * variable is updated by all functions generating random objects.  Its
 * initial value has been drawn from /dev/random.
 */
atomic_ullong random_seed = 0x70184fb2;

/*
 * Use the xorshift random number generator to generate a random number
 * between 0 and 2^64 - 1.  We use an atomic exchange operation to make
 * sure that each result of the RNG is consumed exactly once.
 */
extern unsigned long long
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
	__uint128_t rnd;
	unsigned long long rnd1, rnd2;
	size_t i, j, parity = 0;
	int ipos, jpos;

	/* silence valgrind as we technically read uninitialized values */
	memset(p, 0, sizeof *p);

	/*
	 * Since we consume many of the 128 bits of entropy we get, we
	 * must be careful to avoid modulo bias.  This is done by
	 * capping the range of random numbers we admit upwards to get a
	 * clean multiple of the range we are interested in.
	 */
	do {
		rnd1 = xorshift();
		rnd2 = xorshift_step_alt(rnd1);
		rnd = (__uint128_t)rnd1 << 64 | rnd2;

		/* 23 * 24 * 25! */
	} while (rnd >= (__uint128_t)39742454749 * 23 * 24 *
		    2432902008176640000ULL * 6375600ULL /* 25! */);

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
	unsigned long long lseed, entropy;
	int i, n_move, reservoir = 0;
	signed char moves[4];

	entropy = lseed = xorshift();
	reservoir = 32;

	st = fsm_start_state(zero_location(p));

	while (steps > 0) {
		n_move = fsm_get_moves(moves, st, fsm);

		switch (n_move) {
		case 0:	return (0); /* cannot proceed */

		case 1: i = 0; /* no choice to make */
			break;

		default:
			do {
				if (reservoir == 0) {
					entropy = lseed = xorshift_step_alt(lseed);
					reservoir = 32;
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

	return (1);
}
