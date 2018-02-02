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

/* compact.c -- compact puzzle representation */

#ifndef COMPACT_H
#define COMPACT_H

#include "puzzle.h"

/*
 * To save storage, we store puzzles using a compact representation with
 * five bits per entry, not storing the position of the zero tile.
 * Additionally, four bits are used to store all moves that lead back to
 * the previous generation.  This leads to 24 * 5 + 4 = 124 bits of
 * storage being required in total, split into two 64 bit quantities.
 * lo and hi store 12 tiles @ 5 bits each, lo additionally stores 4 move
 * mask bits in its least significant bits.
 */
struct compact_puzzle {
	unsigned long long lo, hi;
};

#define MOVE_MASK 0xfull

/* compact.c */
extern void	pack_puzzle(struct compact_puzzle *restrict, const struct puzzle *restrict);
extern void	pack_puzzle_masked(struct compact_puzzle *restrict, const struct puzzle *restrict, int);
extern void	unpack_puzzle(struct puzzle *restrict, const struct compact_puzzle *restrict);
extern int	compare_cp(const void *, const void *);

#endif /* COMPACT_H */
