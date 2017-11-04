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
