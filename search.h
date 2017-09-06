/* search.h -- searching algorithms */

#ifndef SEARCH_H
#define SEARCH_H

#include <stdio.h>

#include "pdb.h"

/*
 * All search functions receive an array to store the path they found
 * as an argument and return the length of that path they found or
 * NO_PATH if no path could be found.
 */
enum {
	SEARCH_PATH_LEN = 512 - sizeof(size_t),
	SEARCH_NO_PATH = -1,
};

struct path {
	size_t pathlen;
	unsigned char moves[SEARCH_PATH_LEN];
};

extern size_t	search_ida(struct patterndb **, size_t, const struct puzzle *, struct path *, FILE *);

#endif /* SEARCH_H */
