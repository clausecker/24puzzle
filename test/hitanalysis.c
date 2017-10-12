/* hitanalysis.c -- analyze how tile sets hit tile pairs */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "tileset.h"
#include "catalogue.h"
#include "puzzle.h"

/*
 * Korf and Taylor noted that the entanglement between pairs of tiles
 * makes a good heuristic.  In this file, we analyze which tile pairs
 * are hit by which heuristic.  The goal is to create a catalogue of
 * PDBs such that all pairs of tiles are hit by some PDB set in the
 * catalogue.  Hopefully, this idea allows us to find good PDB sets.
 */
static void
visualise_covering(const tileset *tilesets, size_t n_ts)
{
	size_t x, y, i;
	size_t accum;
	tileset pair;

	/* first, print coordinate labels */
	printf("  ");

	for (i = 1; i < TILE_COUNT; i++)
		printf("%2zu", i);

	printf("\n");

	for (y = 1; y < TILE_COUNT; y++) {
		printf("%2zu ", y);

		for (x = 1; x < TILE_COUNT; x++) {
			pair = 1 << x | 1 << y;
			accum = 0;
			for (i = 0; i < n_ts; i++)
				accum += pair == (tilesets[i] & pair);

			if (accum > 15)
				accum = 15;

			printf("%c%c", " 123456789ABCDEF"[accum], x == TILE_COUNT - 1 ? '\n' : ' ');
		}
	}
}

extern int
main(int argc, char *argv[])
{
	size_t i, n_ts;
	tileset tilesets[CATALOGUE_PDBS_LEN];

	if (argc > 1 + CATALOGUE_PDBS_LEN) {
		fprintf(stderr, "Too many tile sets, up to %d allowed.\n", CATALOGUE_PDBS_LEN);
	}

	n_ts = argc - 1;
	for (i = 0; i < n_ts; i++)
		if (tileset_parse(tilesets + i, argv[i + 1]) != 0) {
			fprintf(stderr, "Invalid tileset: %s\n", argv[i + 1]);
			return (EXIT_FAILURE);
		}

	visualise_covering(tilesets, n_ts);

	return (EXIT_SUCCESS);
}
