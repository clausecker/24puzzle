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

/* randompdb.c -- Generate a random PDB partitioning */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tileset.h"
#include "random.h"

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-0] n1 n2 ...\n", argv0);

	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	struct timespec now;
	size_t i;
	tileset ts, max, used = tileset_add(EMPTY_TILESET, ZERO_TILE);
	tileset zero_tile = EMPTY_TILESET;
	unsigned limit, rnd;
	int n_tile, free_spots, optchar;
	char tsstr[TILESET_LIST_LEN];

	while (optchar = getopt(argc, argv, "0"), optchar != -1)
		switch (optchar) {
		case '0':
			zero_tile = tileset_add(zero_tile, ZERO_TILE);
			break;

		default:
			usage(argv[0]);
		}

	/* get entropy */
	if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
		perror("clock_gettime");
		return (EXIT_FAILURE);
	}

	set_seed(now.tv_sec + (unsigned long long)now.tv_nsec << 32);

	for (i = optind; i < argc; i++) {
		free_spots = tileset_count(tileset_complement(used));
		n_tile = atoi(argv[i]);

		if (n_tile < 1 || n_tile > free_spots) {
			fprintf(stderr, "Tile count out of range: %s\n", argv[i]);
			return (EXIT_FAILURE);
		}

		max = (1 << n_tile) - 1 << free_spots - n_tile;
		limit = tileset_rank(max) + 1;

		rnd = random32() % limit;
		tileset_unrank_init(n_tile);
		ts = pdep(tileset_complement(used), tileset_unrank(n_tile, rnd));

		assert(tileset_count(ts) == n_tile);
		assert(tileset_intersect(ts, used) == EMPTY_TILESET);

		used = tileset_union(ts, used);

		tileset_list_string(tsstr, tileset_union(ts, zero_tile));
		printf("%s%c", tsstr, i + 1 == argc ? '\n' : ' ');
	}
}
