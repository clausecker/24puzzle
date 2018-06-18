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

/* pdbsearch.c -- search solutions using a PDB */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "search.h"
#include "catalogue.h"
#include "fsm.h"
#include "pdb.h"
#include "index.h"
#include "puzzle.h"
#include "tileset.h"

enum { CHUNK_SIZE = 1024 };

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-Fit] [-j nproc] [-m fsmfile] [-d pdbdir] catalogue\n", argv0);

	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	const struct fsm *fsm = &fsm_simple;
	struct pdb_catalogue *cat;
	struct path path;
	struct puzzle p;
	FILE *fsmfile;
	int optchar, catflags = 0, idaflags = IDA_VERBOSE, transpose = 0;
	char linebuf[1024], pathstr[PATH_STR_LEN], *pdbdir = NULL;

	while (optchar = getopt(argc, argv, "Fd:ij:m:t"), optchar != -1)
		switch (optchar) {
		case 'F':
			idaflags |= IDA_LAST_FULL;
			break;

		case 'd':
			pdbdir = optarg;
			break;

		case 'i':
			catflags |= CAT_IDENTIFY;
			break;

		case 'j':
			pdb_jobs = atoi(optarg);
			if (pdb_jobs < 1 || pdb_jobs > PDB_MAX_JOBS) {
				fprintf(stderr, "Number of threads must be between 1 and %d\n",
				    PDB_MAX_JOBS);
				return (EXIT_FAILURE);
			}

			break;

		case 'm':
			fprintf(stderr, "Loading finite state machine file %s\n", optarg);
			fsmfile = fopen(optarg, "rb");
			if (fsmfile == NULL) {
				perror(optarg);
				return (EXIT_FAILURE);
			}

			fsm = fsm_load(fsmfile);
			if (fsm == NULL) {
				perror("fsm_load");
				return (EXIT_FAILURE);
			}

			fclose(fsmfile);
			break;

		case 't':
			transpose = 1;
			break;

		default:
			usage(argv[0]);
		}

	if (argc != optind + 1)
		usage(argv[0]);

	cat = catalogue_load(argv[optind], pdbdir, catflags, stderr);
	if (cat == NULL) {
		perror("catalogue_load");
		return (EXIT_FAILURE);
	}

	if (transpose && catalogue_add_transpositions(cat) != 0) {
		perror("catalogue_add_transpositions");
		fprintf(stderr, "Proceeding anyway...\n");
	}

	for (;;) {
		printf("Enter instance to solve:\n");
		if (fgets(linebuf, sizeof linebuf, stdin) == NULL)
			if (ferror(stdin)) {
				perror("fgets");
				return (EXIT_FAILURE);
			} else
				return (EXIT_SUCCESS);

		if (puzzle_parse(&p, linebuf) != 0)
			continue;

		if (puzzle_parity(&p) != 0) {
			printf("Puzzle unsolvable.\n");
			continue;
		}

		fprintf(stderr, "Solving puzzle...\n");
		search_ida(cat, fsm, &p, &path, idaflags);
		path_string(pathstr, &path);
		printf("Solution found: %s\n", pathstr);
	}
}
