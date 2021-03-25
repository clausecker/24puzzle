/*-
 * Copyright (c) 2018, 2020, 2021 Robert Clausecker. All rights reserved.
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

/* explore.c -- interactively explore puzzles */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>

#include "puzzle.h"
#include "search.h"

extern int
main(int argc, char *argv[])
{
	struct puzzle p = solved_puzzle;
	int dest, n_moves = 1;
	char puzstr[PUZZLE_STR_LEN];

	/*
	 * if a command line argument is given, interpret it
	 * as a solution and go to the puzzle solved by it.
	 * Naturally, the last step of the path cannot be undone.
	 */
	if (argc > 2) {
		fprintf(stderr, "usage: %s [solution]\n", argv[0]);
		return (EXIT_FAILURE);
	}

	if (argc == 2) {
		struct path path;
		int i;
		char *end;

		end = path_parse(&path, argv[1]);
		if (*end != '\0') {
			fprintf(stderr, "cannot parse path: %s\n", argv[1]);
			return (EXIT_FAILURE);
		}

		for (i = 0; i < path.pathlen; i++)
			move(&p, path.moves[path.pathlen - i - 1]);
	}

	for (;;) {
		puzzle_visualization(puzstr, &p);
		fputs(puzstr, stdout);

		printf("move %3d: ", n_moves);
		fflush(stdout);

		switch (scanf("%d", &dest)) {
		case EOF:
			puzzle_string(puzstr, &p);
			printf("\n%s\n", puzstr);
			return (EXIT_SUCCESS);

		case 0:
			continue;

		case 1:
			if (dest < 0 || dest >= TILE_COUNT)
				continue;

			move(&p, p.tiles[dest]);
			n_moves++;
		}
	}
}
