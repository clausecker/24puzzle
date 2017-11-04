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

/* genpdb.c -- generate a PDB */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tileset.h"
#include "index.h"
#include "pdb.h"

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-f file] [-t tile,tile,...] [-j nproc]\n", argv0);

	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	struct patterndb *pdb;
	tileset ts = DEFAULT_TILESET;
	int optchar;
	const char *fname = NULL;
	FILE *f = NULL;

	while (optchar = getopt(argc, argv, "f:j:t:"), optchar != -1)
		switch (optchar) {
		case 'f':
			fname = optarg;
			break;

		case 'j':
			pdb_jobs = atoi(optarg);
			if (pdb_jobs < 1 || pdb_jobs > PDB_MAX_JOBS) {
				fprintf(stderr, "Number of threads must be between 1 and %d\n",
				    PDB_MAX_JOBS);
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

	if (tileset_count(ts) >= 16) {
		fprintf(stderr, "%d tiles are too many tiles. Up to 15 tiles allowed.\n",
		    tileset_count(ts));
		return (EXIT_FAILURE);
	}

	if (fname != NULL) {
		f = fopen(fname, "wb");
		if (f == NULL) {
			perror("fopen");
			return (EXIT_FAILURE);
		}
	}

	pdb = pdb_allocate(ts);
	if (pdb == NULL) {
		perror("pdb_allocate");
		return (EXIT_FAILURE);
	}

	pdb_generate(pdb, stderr);

	if (f != NULL && pdb_store(f, pdb) != 0) {
		perror("pdb_store");
		return (EXIT_FAILURE);
	}

	if (f != NULL && fclose(f) == EOF) {
		perror("fclose");
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}
