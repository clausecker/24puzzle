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

/* bitpdb.c -- reduce PDBs to one bit per entry */

/*
 * For the sake of IDA*, we only need to know if a move is predicted as
 * getting closer to the goal or farther away.  If the PDB represents a
 * consistent heuristic, this means that we only need to care about the
 * least two bits of the h value.  The following transisitions are
 * possible in a bipartite graph:
 *
 *     00 -> 01    sad          10 -> 11    sad
 *     00 -> 10    happy        10 -> 01    happy
 *     01 -> 10    sad          11 -> 00    sad
 *     01 -> 11    happy        11 -> 10    happy
 *
 * when computing the h value for a configuration, we can find the least
 * significant bit by observing the configuration's parity.  Thus, only
 * the second to least significant bit has to be stored in the PDB.  The
 * other bits can be reconstructed if needed by following happy moves in
 * the PDB's quotient graph until the goal is reached.
 *
 * Note that this technique probably doesn't work with identified PDBs
 * due to their inconsistency.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>

#include "pdb.h"
#include "index.h"
#include "tileset.h"

/*
 * Copy the second-to-least signficiant bit of each byte in pdb to
 * ofile in liddle-endian order.
 */
static void
write_bitpdb(FILE *ofile, struct patterndb *pdb)
{
	unsigned char buf, *data = (unsigned char *)pdb->data;
	size_t n = search_space_size(&pdb->aux), i, j;

	flockfile(ofile);

	/* assumes 8 divides n which holds for two tiles or more */
	for (i = 0; i < n; i += 8) {
		buf = 0;
		for (j = 0; j < 8; j++)
			buf |= (data[i + j] >> 1 & 1) << j;

		putc_unlocked(buf, ofile);
	}

	funlockfile(ofile);
}

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s -t tileset [-o file.bpdb] [file.pdb]\n", argv0);
	exit(EXIT_FAILURE);
}


extern int
main(int argc, char *argv[])
{
	struct patterndb *pdb;
	FILE *f = stdin, *o = stdout;
	tileset ts = DEFAULT_TILESET;
	int optchar;

	while (optchar = getopt(argc, argv, "t:o:"), optchar != -1)
		switch (optchar) {
		case 'o':
			o = fopen(optarg, "wb");
			if (o == NULL) {
				perror(optarg);
				return (EXIT_FAILURE);
			}

			break;

		case 't':
			if (tileset_parse(&ts, optarg) != 0) {
				fprintf(stderr, "Cannot parse tile set: %s\n", optarg);
				return (EXIT_FAILURE);
			}

			break;

		case '?':
		case ':':
			usage(argv[0]);
		}

	switch (argc - optind) {
	case 0:
		break;

	case 1:
		f = fopen(argv[optind], "rb");
		if (f == NULL) {
			perror(argv[optind]);
			return (EXIT_FAILURE);
		}

		break;

	default:
		usage(argv[0]);
	}

	pdb = pdb_mmap(ts, fileno(f), PDB_MAP_RDONLY);
	if (pdb == NULL) {
		perror("pdb_mmap");
		return (EXIT_FAILURE);
	}

	write_bitpdb(o, pdb);

	return (EXIT_SUCCESS);
}
