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

/* pdbquality.c -- compute and print the quality of a PDB */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <unistd.h>

#include "pdb.h"
#include "tileset.h"

static noreturn void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-v] [-t tile,...] [-j nproc] [file.pdb]\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	struct patterndb *pdb;
	FILE *pdbfile;
	tileset ts = DEFAULT_TILESET;
	int optchar, verbose = 0, jobs = pdb_jobs;
	char tsstr[TILESET_LIST_LEN];

	while (optchar = getopt(argc, argv, "j:t:v"), optchar != -1)
		switch (optchar) {
		case 'j':
			jobs = atoi(optarg);
			if (jobs < 1 || jobs > PDB_MAX_JOBS) {
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

		case 'v':
			verbose = 1;
			break;

		default:
			usage(argv[0]);
		}

	pdb_jobs = jobs;

	switch (argc - optind) {
	case 1:
		pdbfile = fopen(argv[optind], "rb");
		if (pdbfile == NULL) {
			perror(argv[optind]);
			return (EXIT_FAILURE);
		}

		pdb = pdb_mmap(ts, fileno(pdbfile), PDB_MAP_RDONLY);
		if (pdb == NULL) {
			perror("pdb_mmap");
			return (EXIT_FAILURE);
		}

		fclose(pdbfile);
		break;

	case 0:
		pdb = pdb_allocate(ts);
		if (pdb == NULL) {
			perror("pdb_allocate");
			return (EXIT_FAILURE);
		}

		pdb_generate(pdb, verbose ? stderr : NULL);
		break;

	default:
		usage(argv[0]);
	}

	tileset_list_string(tsstr, ts);
	printf("%.18f %.18e %s\n", pdb_h_average(pdb), pdb_eta(pdb), tsstr);

	return (EXIT_SUCCESS);
}
