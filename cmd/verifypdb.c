/* validatepdb.c -- Validate a PDB */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tileset.h"
#include "index.h"
#include "pdb.h"

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s -f file [-t tile,tile,...]\n", argv0);

	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	tileset ts = 0x00000e7; /* 0 1 2 5 6 7 */
	size_t count, size;
	int optchar, jobs = 1;
	const char *fname = NULL;

	patterndb pdb;
	FILE *f = NULL;

	while (optchar = getopt(argc, argv, "f:j:t:"), optchar != -1)
		switch (optchar) {
		case 'f':
			fname = optarg;
			break;

		case 'j':
			jobs = atoi(optarg);
			if (jobs < 1 || jobs > PDB_MAX_THREADS) {
				fprintf(stderr, "Number of threads must be between 1 and %d\n",
				    PDB_MAX_THREADS);
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
		f = fopen(fname, "rb");
		if (f == NULL) {
			perror("fopen");
			return (EXIT_FAILURE);
		}

	} else {
		fprintf(stderr, "Missing mandatory option -f");
		return (EXIT_FAILURE);
	}

	size = search_space_size(ts);

	pdb = malloc(size);
	if (pdb == NULL) {
		perror("malloc");
		return (EXIT_FAILURE);
	}

	count = fread(pdb, 1, size, f);
	if (count < size) {
		if (ferror(f))
			perror("fread");
		else
			fprintf(stderr, "PDB too short.\n");

		return (EXIT_FAILURE);
	}

	fclose(f);

	return (verify_patterndb(pdb, ts, jobs, stderr));
}
