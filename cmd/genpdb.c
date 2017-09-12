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
