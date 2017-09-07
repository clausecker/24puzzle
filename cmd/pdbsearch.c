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
	fprintf(stderr, "Usage: %s [-i] [-j nproc] tileset ...\n", argv0);

	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	struct patterndb *pdbs[PDB_MAX_COUNT];
	struct path path;
	struct puzzle p;
	size_t i, n_pdb;
	int optchar, identify = 0;
	tileset ts;
	char linebuf[1024], pathstr[PATH_STR_LEN];

	while (optchar = getopt(argc, argv, "ij:"), optchar != -1)
		switch (optchar) {
		case 'i':
			identify = 1;
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

	n_pdb = argc - optind;
	if (n_pdb > PDB_MAX_COUNT) {
		fprintf(stderr, "Up to %d PDBs are allowed.\n", PDB_MAX_COUNT);
		return (EXIT_FAILURE);
	}

	for (i = 0; i < n_pdb; i++) {
		if (tileset_parse(&ts, argv[optind + i]) != 0) {
			fprintf(stderr, "Invalid tileset: %s\n", argv[optind + i]);
			return (EXIT_FAILURE);
		}

		if (identify)
			ts = tileset_add(ts, ZERO_TILE);

		pdbs[i] = pdb_allocate(ts);
		if (pdbs[i] == NULL) {
			perror("pdb_allocate");
			return (EXIT_FAILURE);
		}
	}

	/* split up allocation and generation so we know up front if we have enough RAM */
	for (i = 0; i < n_pdb; i++) {
		fprintf(stderr, "Generating PDB for tiles %s\n", argv[optind + i]);
		pdb_generate(pdbs[i], stderr);
		if (identify) {
			fprintf(stderr, "\nIdentifying PDB...\n");
			pdb_identify(pdbs[i]);
		}

		fputs("\n", stderr);
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

		fprintf(stderr, "Solving puzzle using %zu PDBs...\n", n_pdb);
		search_ida(pdbs, n_pdb, &p, &path, stderr);
		path_string(pathstr, &path);
		printf("Solution found: %s\n", pathstr);
	}
}
