#ifndef BUILTINS_H

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
	return ((x + (x >> 4) & 0x0f0f0f0fu) * 0x01010101u >> 24);
#endif
}

#endif /* BUILTINS_H */
