/*-
 * Copyright (c) 2018 Robert Clausecker. All rights reserved.
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

#ifndef RANDOM_H
#define RANDOM_H

#include <stdatomic.h>

#include "puzzle.h"
#include "index.h"
#include "fsm.h"

extern atomic_ullong	random_seed;
extern unsigned long long	xorshift(void);
extern void	random_puzzle(struct puzzle *);
extern void	random_index(const struct index_aux *, struct index *);
extern int	random_walk(struct puzzle *, int, const struct fsm *);

/*
 * A 64 bit xorshift step function with parameters (13, 7, 17).
 */
static inline unsigned long long
xorshift_step(unsigned long long x)
{
	x ^= x >> 13;
	x ^= x << 7;
	x &= 0xffffffffffffffffull;
	x ^= x >> 17;

	return (x);
}

/*
 * A 64 bit xorshift step function with parameters (7, 23, 8).
 * This is used to ensure independence when drawing a seed using
 * xorshift().
 */
static inline unsigned long long
xorshift_step_alt(unsigned long long x)
{
	x ^= x >> 7;
	x ^= x << 23;
	x &= 0xffffffffffffffffull;
	x ^= x >> 8;

	return (x);
}

#endif /* RANDOM_H */
