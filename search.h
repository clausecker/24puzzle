/* search.h -- searching algorithms */

#ifndef SEARCH_H
#define SEARCH_H

#include <stdio.h>

#include "catalogue.h"
#include "pdb.h"

/*
 * All search functions receive an array to store the path they found
 * as an argument and return the length of that path they found or
 * NO_PATH if no path could be found.
 */
enum {
	SEARCH_PATH_LEN = 512 - sizeof(size_t),
	SEARCH_NO_PATH = -1,
	PATH_STR_LEN = SEARCH_PATH_LEN * 3,
};

struct path {
	size_t pathlen;
	unsigned char moves[SEARCH_PATH_LEN];
};

/* search.c */
extern void	path_string(char[PATH_STR_LEN], const struct path *);

/* various */
extern unsigned long long	search_ida(struct pdb_catalogue *, const struct puzzle *, struct path *, FILE *);

#endif /* SEARCH_H */
