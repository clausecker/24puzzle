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

#endif /* BUILTINS_H */
