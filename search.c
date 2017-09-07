/* search.c -- utility functions for search.h */

#include <stdio.h>

#include "search.h"

/*
 * Convert path to a string.  The buffer must be at least
 * SEARCH_PATH_LEN bytes long, the resulting string has the form
 * tile,tile,...
 */
extern void
path_string(char str[PATH_STR_LEN], const struct path *path)
{
	size_t i, offset = 0;

	for (i = 0; i < path->pathlen; i++) {
		offset += sprintf(str + offset, "%d", path->moves[i]);
		str[offset++] = ',';
	}

	str[offset - 1] = '\0';
}
