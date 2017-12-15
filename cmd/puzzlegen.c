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
		puts(puzzlestr);
	}

	return (EXIT_SUCCESS);
}
