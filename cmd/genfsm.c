/*-
 * Copyright (c) 2018 Robert Clausecker. All rights reserved.
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

/* genfsm.c -- generate a finite state machine */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "puzzle.h"
#include "compact.h"
#include "search.h"

/*
 * Determine a path leading to configuration cp, the last move of which
 * is last_move.  It is assumed that the path has length len.  rounds is
 * used to look up nodes along the way with the first len entries of
 * rounds being used.
 */
static void
find_path(struct path *path, const struct compact_puzzle *cp, int last_move,
    const struct cp_slice *rounds, size_t len)
{
	/* TODO */
}

/*
 * Search through expansion round len - 1 and print out all new half
 * loops to fsmfile.  We only print those loops spanning the entirety of
 * the search tree, i.e. where the two paths only join in the root node.
 * For each half loop, one branch is choosen as the canonical path.  The
 * other branches are pruned from the search tree.  For this reason,
 * the next round must be expanded before calling this function.  This
 * function assumes that the penultimate entry in rounds has already
 * been deduplicated.
 */
static void
do_loops(FILE *fsmfile, struct cp_slice *rounds, size_t len)
{
	struct compact_puzzle *cps = rounds[len - 1].data;
	size_t i, n_cps = rounds[len - 1].len;
	int movemask;

	for (i = 0; i < n_cps; i++) {
		movemask = cps[i].lo & MOVE_MASK;
		if (popcount(movemask) <= 1)
			continue;

		
	}
}



static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-l limit] [-s start_tile] [fsm]\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	FILE *fsmfile;
	int optchar, limit = PDB_HISTOGRAM_LEN, start_tile = 0;

	while (optchar = getopt(argc, argv, "l:s:"), optchar != -1)
		switch (optchar) {
		case 'l':
			limit = atoi(optarg);
			if (limit > PDB_HISTOGRAM_LEN)
				limit = PDB_HISTOGRAM_LEN;
			else if (limit < 0) {
				fprintf(stderr, "Limit must not be negative: %s\n", optarg);
				usage(argv[0]);
			}

			break;

		case 's':
			start_tile = atoi(optarg);
			if (start_tile < 0 || start_tile >= TILE_COUNT) {
				fprintf(stderr, "Start tile out of range, must be between 0 and 24: %s\n", optarg);
				usage(argv[0]);
			}

			break;

		default:
			usage(argv[0]);
		}

	switch (argc - optind) {
	case 0:
		fsmfile = stdout;
		break;

	case 1:
		fsmfile = fopen(argv[optind], "w");
		if (fsmfile == NULL) {
			perror(argv[optind]);
			return (EXIT_FAILURE);
		}

		break;

	default:
		usage(argv[0]);
		break;
	}
}
