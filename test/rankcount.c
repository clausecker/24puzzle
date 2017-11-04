/* rankcount.c -- count the number equivalence classes for each map rank */
#include <stdlib.h>
#include <stdio.h>

#include "tileset.h"
#include "index.h"

extern int
main(int argc, char *argv[])
{
	struct index_aux aux;
	size_t max_eqclass, i;
	int tilecount;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s tilecount\n", argv[0]);
		return (EXIT_FAILURE);
	}

	tilecount = atoi(argv[1]);
	if (tilecount < 0 || tilecount >= TILE_COUNT) {
		fprintf(stderr, "Invalid tile count %s\n", argv[1]);
		return (EXIT_FAILURE);
	}

	/* +1 for zero tile */
	make_index_aux(&aux, tileset_least(tilecount + 1));
	max_eqclass = 0;

	for (i = 0; i < aux.n_maprank; i++)
		if (eqclass_count(&aux, i) > max_eqclass)
			max_eqclass = eqclass_count(&aux, i);

	printf("%d %zu %zu %.2f %zu\n", tilecount, (size_t)aux.n_maprank * aux.n_perm,
	    search_space_size(&aux), (double)eqclass_total(&aux) / aux.n_maprank,
	    max_eqclass);

	return (EXIT_SUCCESS);
}
