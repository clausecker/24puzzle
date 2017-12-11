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

/* pdbmatch.c -- find optimal PDB partitionings */

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "heuristic.h"
#include "tileset.h"
#include "match.h"
#include "transposition.h"

static struct puzzle	 *read_puzzles(size_t *, FILE *);
static unsigned char	**lookup_puzzles(const struct puzzle *, size_t, const char *);
static void		  lookup_pattern(unsigned char **, tileset, const struct puzzle *, size_t, const char *);
static void		  find_matches(struct match *, const struct puzzle *, unsigned char **, size_t);
static void		  print_matches(struct match *, const struct puzzle *, size_t);

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s -d pdbdir [puzzles]\n", argv0);

	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	FILE *puzzlefile = NULL;
	struct puzzle *puzzles;
	struct match *matches;
	size_t n_puzzle;
	int optchar;
	char *pdbdir = NULL;
	unsigned char **vs;

	while (optchar = getopt(argc, argv, "d:"), optchar != -1)
		switch (optchar) {
		case 'd':
			pdbdir = optarg;
			break;

		default:
			usage(argv[0]);
		}

	switch (argc - optind) {
	case 0:
		puzzlefile = stdin;
		break;

	case 1:
		puzzlefile = fopen(argv[optind], "r");
		if (puzzlefile == NULL) {
			perror(argv[optind]);
			return (EXIT_FAILURE);
		}

		break;

	default:
		usage(argv[0]);
	}

	if (pdbdir == NULL)
		usage(argv[0]);

	puzzles = read_puzzles(&n_puzzle, puzzlefile);
	fclose(puzzlefile);
	matches = malloc(n_puzzle * sizeof *matches);
	if (matches == NULL) {
		perror("malloc");
		return (EXIT_FAILURE);
	}

	vs = lookup_puzzles(puzzles, n_puzzle, pdbdir);
	find_matches(matches, puzzles, vs, n_puzzle);
	print_matches(matches, puzzles, n_puzzle);

	return (EXIT_SUCCESS);
}

static struct puzzle *
read_puzzles(size_t *n_puzzle, FILE *puzzlefile)
{
	struct puzzle *puzzles;
	size_t cap = 64, n_linebuf;
	char *linebuf = NULL;

	*n_puzzle = 0;
	puzzles = malloc(cap * sizeof *puzzles);
	if (puzzles == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	while (getline(&linebuf, &n_linebuf, puzzlefile) != -1) {
		if (*n_puzzle >= cap) {
			/* Fibonacci growth */
			cap = cap * 13 / 8;
			puzzles = realloc(puzzles, cap);
			if (puzzles == NULL) {
				perror("realloc");
				exit(EXIT_FAILURE);
			}
		}

		if (puzzle_parse(puzzles + (*n_puzzle)++, linebuf) != 0) {
			fprintf(stderr, "Invalid puzzle: %s", linebuf);
			exit(EXIT_FAILURE);
		}
	}

	if (ferror(puzzlefile)) {
		perror("getline");
		exit(EXIT_FAILURE);
	}

	free(linebuf);

	return (puzzles);
}

static unsigned char **
lookup_puzzles(const struct puzzle *puzzles, size_t n_puzzles, const char *pdbdir)
{
	size_t i;
	unsigned char **vs;

	vs = malloc(n_puzzles * sizeof *vs);
	if (vs == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < n_puzzles; i++) {
		vs[i] = matchv_allocate();
		if (vs[i] == NULL) {
			perror("matchv_allocate");
			exit(EXIT_FAILURE);
		}
	}

	tileset_unrank_init(6);

	for (i = 0; i < MATCH_SIZE; i++)
		lookup_pattern(vs, tileset_unrank(6, (tsrank)i) << 1,
		    puzzles, n_puzzles, pdbdir);

	return (vs);
}

static void
lookup_pattern(unsigned char **vs, tileset ts, const struct puzzle *puzzles,
    size_t n_puzzle, const char *pdbdir)
{
	struct heuristic heu, morphheu;
	size_t j;
	unsigned i;

	if (canonical_automorphism(ts) != 0)
		return;

	if (heu_open(&heu, pdbdir, ts, "zbpdb.zst",
	    HEU_NOMORPH | HEU_SIMILAR | HEU_VERBOSE) != 0) {
		perror("heu_open");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < AUTOMORPHISM_COUNT; i++) {
		if (!is_admissible_morphism(ts, i))
			continue;

		heu_morph(&morphheu, &heu, i);
		for (j = 0; j < n_puzzle; j++)
			match_amend(vs[j], puzzles + j, &morphheu);
	}

	heu_free(&heu);
}

static void
find_matches(struct match *matches, const struct puzzle *puzzles,
    unsigned char **vs, size_t n_puzzle)
{
	size_t i;

	for (i = 0; i < n_puzzle; i++)
		if (match_find_best(matches + i, vs[i]) == 0) {
			perror("match_find_best");
			exit(EXIT_FAILURE);
		}
}

static void
print_matches(struct match *matches, const struct puzzle *puzzles, size_t n_puzzle)
{
	size_t i;
	int hval;
	char puzstr[PUZZLE_STR_LEN], tsstr[TILESET_LIST_LEN][4];

	for (i = 0; i < n_puzzle; i++) {
		puzzle_string(puzstr, puzzles + i);
		tileset_list_string(tsstr[0], matches[i].ts[0]);
		tileset_list_string(tsstr[1], matches[i].ts[1]);
		tileset_list_string(tsstr[2], matches[i].ts[2]);
		tileset_list_string(tsstr[3], matches[i].ts[3]);
		hval = matches[i].hval[0] + matches[i].hval[1]
		    + matches[i].hval[2] + matches[i].hval[3];

		printf("%s %3d %2d %2d %2d %2d %s %s %s %s\n",
		    puzstr, hval, matches[i].hval[0], matches[i].hval[1],
		    matches[i].hval[2], matches[i].hval[3], tsstr[0],
		    tsstr[1], tsstr[2], tsstr[3]);
	}
}
