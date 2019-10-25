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

/* search.h -- searching algorithms */

#ifndef SEARCH_H
#define SEARCH_H

#include <stdio.h>

#include "puzzle.h"
#include "catalogue.h"
#include "fsm.h"

/*
 * All search functions receive an array to store the path they found
 * as an argument and return the length of that path they found or
 * NO_PATH if no path could be found.
 */
enum {
	SEARCH_PATH_LEN = PDB_HISTOGRAM_LEN,
	SEARCH_NO_PATH = -1,
	PATH_STR_LEN = SEARCH_PATH_LEN * 3,
};

/*
 * Flags for search_ida_bounded()
 */
enum {
	/* perform the last round in full */
	IDA_LAST_FULL = 1 << 0,
	/* print status information to stderr */
	IDA_VERBOSE = 1 << 1,
	/* verify that a correct path was found */
	IDA_VERIFY = 1 << 2,
};

struct path {
	size_t pathlen;
	unsigned char moves[SEARCH_PATH_LEN];
};

/* search.c */
extern void	 path_string(char[PATH_STR_LEN], const struct path *);
extern char	*path_parse(struct path *, const char *);
extern void	 path_walk(struct puzzle *, const struct path *);

/* various */
extern unsigned long long	search_ida(struct pdb_catalogue *, const struct fsm *, const struct puzzle *, struct path *, void (*)(const struct path *, void *), void *, int);
extern unsigned long long	search_ida_bounded(struct pdb_catalogue *, const struct fsm *, const struct puzzle *, size_t, struct path *, void (*)(const struct path *, void *), void *, int);

#endif /* SEARCH_H */
