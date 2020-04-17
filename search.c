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

/* search.c -- utility functions for search.h */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>

#include "puzzle.h"
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

/*
 * Parse a path from the prefix of str.  Return a pointer to the first
 * character that does not belong to the path or NULL if no path could
 * be parsed.  In this case, the content of *path is undefined.
 */
extern char *
path_parse(struct path *path, const char *strarg)
{
	unsigned long move;
	size_t len = 1;
	char *str = (char *)strarg; /* avoid silly warnings */

	move = strtol(str, &str, 10);
	path->moves[0] = move;
	if (move >= TILE_COUNT || str == NULL)
		return (NULL);

	while (str[0] == ',') {
		move = strtol(++str, &str, 10);
		path->moves[len++] = move;
		if (move >= TILE_COUNT || str == NULL)
			return (NULL);
	}

	path->pathlen = len;
	return (str);
}

/*
 * Perform the moves in path on p.
 */
extern void
path_walk(struct puzzle *p, const struct path *path)
{
	size_t i;

	for (i = 0; i < path->pathlen; i++)
		move(p, path->moves[i]);
}
