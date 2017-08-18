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
	fprintf(stderr, "Usage: %s [-i|-o file] -f file [-t tile,tile,...] [-j nproc]\n", argv0);

	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	tileset ts = 0x00000e7; /* 0 1 2 5 6 7 */
	size_t count, size;
	int optchar, in_place = 0;
	const char *fname = NULL, *oname = NULL;

	patterndb pdb;
	FILE *f, *o = NULL;

	while (optchar = getopt(argc, argv, "f:ij:o:t:"), optchar != -1)
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

		case 'i':
			in_place = 1;
			break;

		case 'o':
			oname = optarg;
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

	if (fname == NULL)
		usage(argv[0]);

	if (in_place && oname != NULL) {
		fprintf(stderr, "Options -i and -o are mutually exclusive.\n");
		usage(argv[0]);
	}

	f = fopen(fname, in_place ? "r+b" : "rb");
	if (f == NULL) {
		perror("fopen");
		return (EXIT_FAILURE);
	}

	if (in_place)
		o = f;
	else if (oname != NULL) {
		o = fopen(oname, "wb");
		if (o == NULL) {
			perror("fopen");
			return (EXIT_FAILURE);
		}
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

	if (in_place)
		rewind(f);
	else
		fclose(f);

	reduce_patterndb(pdb, ts, stderr);

	if (o != NULL) {
		count = fwrite(pdb, 1, size, o);

		if (count < size) {
			if (ferror(o))
				perror("fwrite");
			else
				fprintf(stderr, "End of file writing PDB.\n");

			return (EXIT_FAILURE);
		}
	}

	fclose(o);

	return (EXIT_SUCCESS);
}
