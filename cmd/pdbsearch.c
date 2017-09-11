/* pdbsearch.c -- search solutions using a PDB */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "search.h"
#include "pdb.h"
#include "index.h"
#include "puzzle.h"
#include "tileset.h"

enum { CHUNK_SIZE = 1024 };

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-j nproc] [-d pdbdir] catalogue\n", argv0);

	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	struct pdb_catalogue *cat;
	struct path path;
	struct puzzle p;
	int optchar;
	char linebuf[1024], pathstr[PATH_STR_LEN], *pdbdir = NULL;

	while (optchar = getopt(argc, argv, "d:j:"), optchar != -1)
		switch (optchar) {
		case 'd':
			pdbdir = optarg;
			break;

		case 'j':
			pdb_jobs = atoi(optarg);
			if (pdb_jobs < 1 || pdb_jobs > PDB_MAX_JOBS) {
				fprintf(stderr, "Number of threads must be between 1 and %d\n",
				    PDB_MAX_JOBS);
				return (EXIT_FAILURE);
			}

			break;

		default:
			usage(argv[0]);
		}

	if (argc != optind + 1)
		usage(argv[0]);

	cat = catalogue_load(argv[optind], pdbdir, stderr);
	if (cat == NULL) {
		perror("catalogue_load");
		return (EXIT_FAILURE);
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

		fprintf(stderr, "Solving puzzle...\n");
		search_ida(cat, &p, &path, stderr);
		path_string(pathstr, &path);
		printf("Solution found: %s\n", pathstr);
	}
}
