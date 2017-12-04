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

/* bitpdbtest -- verify that pdb and bitpdb yield the same h values */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "puzzle.h"
#include "tileset.h"
#include "pdb.h"
#include "bitpdb.h"

static int
compare_hvalues(struct patterndb *pdb, struct bitpdb *bpdb)
{
	struct puzzle p;
	int pdb_hval, bpdb_hval;
	char puzstr[PUZZLE_STR_LEN];

	random_puzzle(&p);
	pdb_hval = pdb_lookup_puzzle(pdb, &p);
	bpdb_hval = bitpdb_lookup_puzzle(bpdb, &p);

	if (pdb_hval == bpdb_hval)
		return (0);

	puzzle_string(puzstr, &p);
	printf("Mismatch! pdb predicts %d but bitpdb predicts %d for puzzle\n%s\n",
	    pdb_hval, bpdb_hval, puzstr);

	return (-1);
}

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-t tile,...] [-n n_puzzle] [-s seed] pdb bitpdb\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	struct patterndb *pdb;
	struct bitpdb *bpdb;
	FILE *pdbfile, *bpdbfile;
	unsigned long long seed = random_seed;
	long i, n_puzzle = 1;
	int optchar;
	tileset ts = DEFAULT_TILESET;

	while (optchar = getopt(argc, argv, "n:s:t:"), optchar != -1)
		switch (optchar) {
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

	if (argc != optind + 2)
		usage(argv[0]);

	pdbfile = fopen(argv[optind], "rb");
	if (pdbfile == NULL) {
		perror(argv[optind]);
		return (EXIT_FAILURE);
	}

	bpdbfile = fopen(argv[optind + 1], "rb");
	if (bpdbfile == NULL) {
		perror(argv[optind] + 1);
		return (EXIT_FAILURE);
	}

	pdb = pdb_mmap(ts, fileno(pdbfile), PDB_MAP_RDONLY);
	if (pdb == NULL) {
		perror(argv[optind]);
		return (EXIT_FAILURE);
	}

	fclose(pdbfile);

	// TODO: use bitpdb_mmap() once implemented
	bpdb = bitpdb_load(ts, bpdbfile);
	if (bpdb == NULL) {
		perror(argv[optind + 1]);
		return (EXIT_FAILURE);
	}

	fclose(bpdbfile);

	for (i = 0; i < n_puzzle; i++)
		if (compare_hvalues(pdb, bpdb))
			return (EXIT_FAILURE);

	return (EXIT_SUCCESS);
}
