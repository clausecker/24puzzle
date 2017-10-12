/* puzzlegen.c -- generate random puzzles */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>

#include "puzzle.h"

extern int
main(int argc, char *argv[])
{
	struct puzzle p;
	size_t i, n_puzzle = 1;
	char puzzlestr[PUZZLE_STR_LEN];

	switch (argc) {
	case 3:
		random_seed = strtoull(argv[2], NULL, 0);
		/* FALLTHROUGH */

	case 2:
		n_puzzle = strtoull(argv[1], NULL, 0);
		/* FALLTHROUGH */

	case 1:
		break;

	default:
		fprintf(stderr, "Usage: %s [n_puzzle [seed]]\n", argv[0]);
		return (EXIT_FAILURE);
	}

	for (i = 0; i < n_puzzle; i++) {
		random_puzzle(&p);
		puzzle_string(puzzlestr, &p);
		printf("%s", puzzlestr);
	}

	return (EXIT_SUCCESS);
}
