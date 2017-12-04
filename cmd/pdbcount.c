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

/* pdbcount.c -- count the number of truly distinct PDBs */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "tileset.h"
#include "transposition.h"
#include "pdb.h"

/*
 * Count how many unique PDBs with n_tiles tiles exist.  If zero is 0,
 * do not account for the zero tile.  If zero is 1, do account for the
 * zero tile.  If print is 0, print statistics at the end.  If print is
 * 1, instead print each canonical PDB.
 */
static void
count_pdbs(int n_tiles, int zero, int print)
{
	size_t i, n = pdbcount[n_tiles], c = 0;
	tileset t = tileset_least(n_tiles), ts;
	char buf[TILESET_LIST_LEN];

	for (i = 0; i < n; i++) {
		ts = t << 1 | zero;
		if (canonical_automorphism(ts) == 0) {
			c++;
			if (print) {
				tileset_list_string(buf, ts);
				puts(buf);
			}
		}

		t = next_combination(t);
	}

	if (print)
		return;

	printf("%s: %20zu / %20zu (%5.2f%%)\n", zero ? "ZPDB" : "APDB",
	    c, (size_t)pdbcount[n_tiles],
	    (100.0 * c) / pdbcount[n_tiles]);
}

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-azp] [n_tiles]\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	int n_tiles = 6, optchar;
	int print = 0, do_apdb = 1, do_zpdb = 1;

	while (optchar = getopt(argc, argv, "apz"), optchar != -1)
		switch (optchar) {
		case 'a':
			do_zpdb = 0;
			break;

		case 'z':
			do_apdb = 0;
			break;

		case 'p':
			print = 1;
			break;

		case ':':
		case '?':
			usage(argv[0]);
		}

	switch (argc - optind) {
	case 0:
		break;

	case 1:
		n_tiles = atoi(argv[optind]);
		break;

	default:
		usage(argv[0]);
	}

	if (do_apdb)
		count_pdbs(n_tiles, 0, print);

	if (do_zpdb)
		count_pdbs(n_tiles, 1, print);

	return (EXIT_SUCCESS);
}
