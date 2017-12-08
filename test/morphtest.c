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

/* morphtest.c -- Verify the correctness of morphed pattern databases */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "puzzle.h"
#include "tileset.h"
#include "pdb.h"
#include "transposition.h"

static int
morphtest(struct patterndb *pdb, struct patterndb *morphpdb, int morphism)
{
	struct puzzle p, morphp;
	int hval, morphhval;
	char puzstr[PUZZLE_STR_LEN], tsstr[TILESET_LIST_LEN];

	random_puzzle(&p);
	hval = pdb_lookup_puzzle(pdb, &p);
	morphp = p;
	morph(&morphp, morphism);
	assert(puzzle_valid(&morphp));
	morphhval = pdb_lookup_puzzle(morphpdb, &morphp);

	if (hval == morphhval)
		return (0);

	puzzle_visualization(puzstr, &p);
	tileset_list_string(tsstr, pdb->aux.ts);
	printf("hval = %d, morphhval = %d, ts = %s\n%s\n", hval, morphhval, tsstr, puzstr);

	puzzle_visualization(puzstr, &morphp);
	tileset_list_string(tsstr, morphpdb->aux.ts);
	printf("morphts = %s\n%s", tsstr, puzstr);

	return (-1);
}

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-d pdbdir] [-t tileset] [-n n_puzzle] [-s seed] morphism\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	struct patterndb *pdb, *morphpdb;
	FILE *pdbfile, *morphfile;
	unsigned long long seed = random_seed;
	long i, n_puzzle = 1;
	int optchar, morphism;
	tileset ts = DEFAULT_TILESET, morphts, zero = 0;
	char *pdbdir = ".", pathbuf[PATH_MAX], tsbuf[TILESET_LIST_LEN];

	while (optchar = getopt(argc, argv, "d:n:s:t:"), optchar != -1)
		switch (optchar) {
		case 'd':
			pdbdir = optarg;
			break;

		case 'n':
			n_puzzle = strtol(optarg, NULL, 0);
			break;

		case 's':
			seed = strtoll(optarg, NULL, 0);
			break;

		case 't':
			if (tileset_parse(&ts, optarg) != 0) {
				printf("Invalid tileset: %s\n", optarg);
				usage(argv[0]);
			}

			break;

		default:
			usage(argv[0]);
		}

	random_seed = seed;

	if (argc != optind + 1)
		usage(argv[0]);


	morphism = atoi(argv[optind]);
	if (!is_admissible_morphism(ts, morphism)) {
		printf("Morphism %d not admissible.\n", morphism);
		return (EXIT_FAILURE);
	}

	if (tileset_has(ts, ZERO_TILE)) {
		zero = tileset_add(zero, ZERO_TILE);
		ts = tileset_remove(ts, ZERO_TILE);
	}

	morphts = tileset_remove(tileset_morph(ts, morphism), ZERO_TILE);

	ts = tileset_union(ts, zero);
	morphts = tileset_union(morphts, zero);

	tileset_list_string(tsbuf, ts);
	snprintf(pathbuf, PATH_MAX, "%s/%s.pdb", pdbdir, tsbuf);
	printf("Loading unmorphed PDB %s\n", pathbuf);
	pdbfile = fopen(pathbuf, "rb");
	if (pdbfile == NULL) {
		perror(pathbuf);
		return (EXIT_FAILURE);
	}

	pdb = pdb_mmap(ts, fileno(pdbfile), PDB_MAP_RDONLY);
	if (pdb == NULL) {
		perror("pdb_mmap");
		return (EXIT_FAILURE);
	}

	fclose(pdbfile);

	tileset_list_string(tsbuf, morphts);
	snprintf(pathbuf, PATH_MAX, "%s/%s.pdb", pdbdir, tsbuf);
	printf("Loading   morphed PDB %s\n", pathbuf);
	morphfile = fopen(pathbuf, "rb");
	if (morphfile == NULL) {
		perror(pathbuf);
		return (EXIT_FAILURE);
	}

	morphpdb = pdb_mmap(morphts, fileno(morphfile), PDB_MAP_RDONLY);
	if (morphpdb == NULL) {
		perror("pdb_mmap");
		return (EXIT_FAILURE);
	}

	fclose(morphfile);

	for (i = 0; i < n_puzzle; i++)
		if (morphtest(pdb, morphpdb, morphism))
			return (EXIT_FAILURE);

	return (EXIT_SUCCESS);
}
