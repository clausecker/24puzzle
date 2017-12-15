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

#ifndef BUILTINS_H
#define BUILTINS_H

/*
 * This header contains code to check if certain builtin functions are
 * available.  If they are not, we try to supply our own
 * implementations if possible.
 */

#ifdef __GNUC__
# define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
# define GCC_VERSION 0
#endif

/* for clang */
#ifndef __has_builtin
# define __has_builtin(x) 0
#endif

/* check if __builtin_popcount() is available */
#ifndef HAS_POPCOUNT
# if __has_builtin(__builtin_popcount) || GCC_VERSION >= 30400
#  define HAS_POPCOUNT 1
# else
#  define HAS_POPCOUNT 0
# endif
#endif

/* check if __builtin_ctz() is available */
#ifndef HAS_CTZ
# if __has_builtin(__builtin_ctz) || GCC_VERSION >= 30400
#  define HAS_CTZ 1
# else
#  define HAS_CTZ 0
# endif
#endif

/* check if __builtin_prefetch() is availabe */
#ifndef HAS_PREFETCH
# if __has_builtin(__builtin_prefetch) || GCC_VERSION >= 30101
#  define HAS_PREFETCH 1
# else
#  define HAS_PREFETCH 0
# endif
#endif

/* check if _pdep_u32() is available */
#ifndef HAS_PDEP
# ifdef __BMI2__
#  include <immintrin.h>
#  define HAS_PDEP 1
# else
#  define HAS_PDEP 0
# endif
#endif

#include <stddef.h>

/*
 * Compute the number of bits set in x.
 */
static inline int
popcount(unsigned x)
{
#if HAS_POPCOUNT == 1
	return (__builtin_popcount(x));
#else
	/* https://graphics.stanford.edu/~seander/bithacks.html */
	x = x - (x >> 1 & 0x55555555u);
	x = (x & 0x33333333u) + (x >> 2 & 0x33333333u);
	x = x + (x >> 4) & 0x0f0f0f0fu;
	return ((x * 0x01010101u & 0xffffffffu) >> 24);
#endif
}

/*
 * Compute the number of trailing zeroes in x.  If x == 0, behaviour is
 * undefined.
 */
static inline int
ctz(unsigned x)
{
#if HAS_CTZ == 1
	return (__builtin_ctz(x));
#else
	/* https://graphics.stanford.edu/~seander/bithacks.html */
	int r = 31;

	x &= -x;

	if (x & 0x0000ffffu)
		r -= 16;
	if (x & 0x00ff00ffu)
		r -=  8;
	if (x & 0x0f0f0f0fu)
		r -=  4;
	if (x & 0x33333333u)
		r -=  2;
	if (x & 0x55555555u)
		r -=  1;

	return (r);
#endif
}

/*
 * dito but for unsigned long long.
 */
static inline int
ctzll(unsigned long long x)
{
#if HAS_CTZ == 1
	return (__builtin_ctzll(x));
#else
	int r = 63;

	x &= -x;

	if (x & 0x00000000ffffffffull)
		r -= 32;
	if (x & 0x0000ffff0000ffffull)
		r -= 16;
	if (x & 0x00ff00ff00ff00ffull)
		r -=  8;
	if (x & 0x0f0f0f0f0f0f0f0full)
		r -=  4;
	if (x & 0x3333333333333333ull)
		r -=  2;
	if (x & 0x5555555555555555ull)
		r -=  1;

	return (r);
#endif
}

/*
 * From x, select the n'th least-significant set bit and return a bit
 * mask containing just that bit.
 */
static inline unsigned
rankselect(unsigned x, unsigned i)
{
#if HAS_PDEP == 1
	return (_pdep_u32(1 << i, x));
#else
	size_t j;

	for (j = 0; j < i; j++)
		x &= x - 1;

	return (x & ~(x - 1));
#endif
}

/*
 * Deposit bits in src into the bits in mask.
 */
static inline unsigned
pdep(unsigned mask, unsigned src)
{
#if HAS_PDEP == 1
	return (_pdep_u32(src, mask));
#else
	/* derived from http://wm.ite.pl/articles/pdep-soft-emu.html */
	unsigned result = 0, idx;

	while (mask != 0) {
		idx = ctz(mask);
		result |= (src & 1) << idx;
		src >>= 1;
		mask &= mask - 1;
	}

	return (result);
#endif
}

/*
 * Prefetch the cache line in which addr is located.  addr does not need
 * to point to valid data necessarily.
 */
static inline
void prefetch(const void *addr)
{
#if HAS_PREFETCH == 1
	__builtin_prefetch(addr);
#else
	(void)addr;
#endif
}

#endif /* BUILTINS_H */
