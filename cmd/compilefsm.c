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

/* compilefsm.c -- compile a finite state machine from loop specifications */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <unistd.h>

#include "puzzle.h"
#include "search.h"

/*
 * The header of a finite state machine file.  A finite state machine
 * file first contains this header and is followed by TILE_COUNT state
 * tables, one for each state.  The offsets member stores the beginnings
 * of these tables, lengths stores the number of 32 bit integers in each
 * table.  struct fsm is the in-memory representation of a struct
 * fsmfile and contains pointers to the tables as well as the actually
 * allocated table sizes.
 */
struct fsmfile {
	/* table offsets in bytes from the beginning of the file */
	off_t offsets[TILE_COUNT];

	/* table lengths in 32 bit words */
	unsigned lengths[TILE_COUNT];
};

struct fsm {
	struct fsmfile header;
	unsigned sizes[TILE_COUNT];
	unsigned (*tables[TILE_COUNT])[4];
};

/*
 * Some special FSM states.  State FSM_BEGIN is the state we start out
 * in, it exists once for every starting square.  FSM_MAX is the highest
 * allowed length for a single state table.  FSM_MATCH is the entry for
 * a match transition, FSM_UNASSIGNED is the entry for an unassigned
 * transition (to be patched later).  These are not enumeration
 * constants as they don't fit in an int.
 */
#define FSM_BEGIN      0x00000000u
#define FSM_MAX_LEN    0xfffffff0u
#define FSM_MATCH      0xfffffffeu
#define FSM_UNASSIGNED 0xffffffffu

/*
 * Add a new entry to the state table for square sq to fsm.  Resize the
 * underlying array if needed and exit if the state table is full.
 * Return the index of the newly allocated state.
 */
static unsigned
addstate(struct fsm *fsm, int sq)
{
	unsigned long long newsize;
	unsigned state;

	assert(0 <= sq && sq < TILE_COUNT);
	state = fsm->header.lengths[sq]++;
	if (state >= FSM_MAX_LEN) {
		fprintf(stderr, "%s: table for square %d full (%u states)\n",
		    __func__, sq, state);
		exit(EXIT_FAILURE);
	}

	/* reallocation needed? */
	if (state >= fsm->sizes[sq]) {
		/* avoid overflow */
		newsize = 1 + 13 * (unsigned long long)fsm->sizes[sq] / 8;
		assert(newsize > fsm->sizes[sq]);
		if (newsize > FSM_MAX_LEN)
			newsize = FSM_MAX_LEN;

		fsm->tables[sq] = realloc(fsm->tables[sq],
		    newsize * sizeof *fsm->tables[sq]);
		if (fsm->tables[sq] == NULL) {
			perror("realloc");
			exit(EXIT_FAILURE);
		}

		fsm->sizes[sq] = newsize;
	}

	fsm->tables[sq][state][0] = FSM_UNASSIGNED;
	fsm->tables[sq][state][1] = FSM_UNASSIGNED;
	fsm->tables[sq][state][2] = FSM_UNASSIGNED;
	fsm->tables[sq][state][3] = FSM_UNASSIGNED;

	return (state);
}

/*
 * Initialize fsm and allocate initial states for each square.
 */
static void
initfsm(struct fsm *fsm)
{
	size_t i;

	memset(fsm, 0, sizeof *fsm);

	for (i = 0; i < TILE_COUNT; i++)
		addstate(fsm, i);
}

/*
 * Compute an index such that get_moves(a)[moveindex(a, b)] == b.
 */
static int
moveindex(int a, int b)
{
	size_t i;
	const signed char *moves = get_moves(a);

	/* TODO: optimize! */
	for (i = 0; i < 4; i++)
		if (moves[i] == b)
			return (i);

	/* no match: programming error */
	assert(0);
}

/*
 * Add a half-loop described by path p to the trie in fsm.  Print an
 * error message and exit if a prefix of p is already in fsm.
 */
static void
addloop(struct fsm *fsm, struct path *p)
{
	size_t state = FSM_BEGIN, i;
	int oldsq = p->moves[0], newsq, move;
	char pathstr[PATH_STR_LEN];

	for (i = 1; i < p->pathlen - 1; i++) {
		newsq = p->moves[i];
		move = moveindex(oldsq, newsq);
		if (fsm->tables[oldsq][state][move] == FSM_UNASSIGNED)
			fsm->tables[oldsq][state][move] = addstate(fsm, newsq);

		if (fsm->tables[oldsq][state][move] == FSM_MATCH) {
			path_string(pathstr, p);
			fprintf(stderr, "%s: prefix already present: ", pathstr);
			p->pathlen = i + 1; /* kludge */
			path_string(pathstr, p);
			fprintf(stderr, "%s\n", pathstr);
			exit(EXIT_FAILURE);
		}

		state = fsm->tables[oldsq][state][move];
		oldsq = newsq;
	}

	newsq = p->moves[i];
	move = moveindex(oldsq, newsq);
	fsm->tables[oldsq][state][move] = FSM_MATCH;
}

/*
 * Read half loops from loopfile and add them to the pristine FSM
 * structure *fsm.  loopfile contains loops as printed by cmd/genloops.
 * After this function ran, the entries in fsm form a trie containing
 * all the half loops from loopfile.  As an extra invariant, no half
 * loop may be the prefix of another one.
 */
static void
readloops(struct fsm *fsm, FILE *loopfile)
{
	struct path p;
	size_t n_linebuf = 0;
	char *linebuf = NULL;
	size_t i;
	int lineno = 1;

	while (getline(&linebuf, &n_linebuf, loopfile) > 0) {
		if (path_parse(&p, linebuf) == NULL) {
			fprintf(stderr, "%s: invalid path on line %d: %s\n",
			    __func__, lineno, linebuf);
			exit(EXIT_FAILURE);
		}

		addloop(fsm, &p);
		lineno++;
	}

	if (ferror(loopfile)) {
		perror("getline");
		exit(EXIT_FAILURE);
	}

	/* release extra storage */
	for (i = 0; i < TILE_COUNT; i++) {
		assert(fsm->header.lengths[i] <= fsm->sizes[i]);

		/* should never fail as we release storage */
		fsm->tables[i] = realloc(fsm->tables[i],
		    fsm->header.lengths[i] * sizeof *fsm->tables[i]);
		assert(fsm->tables[i] != NULL);

		fsm->sizes[i] = fsm->header.lengths[i];
	}

	free(linebuf);
}

/*
 * Find the longest prefix of path[0..pathlen-1] that has an entry in
 * fsm and return the corresponding state.
 */
static unsigned
longestprefix(struct fsm *fsm, const char *path, size_t pathlen)
{
	size_t i, j, idx, zloc;
	unsigned state;

	for (i = 0; i < pathlen; i++) {
		state = 0;
		zloc = path[i];
		for (j = i + 1; j < pathlen; j++) {
			/*
			 * there should be no paths of the form abc in
			 * the FSM where b too is in the FSM.  Todo:
			 * better error message.
			 */
			assert(state < FSM_MAX_LEN);

			idx = moveindex(zloc, path[j]);
			state = fsm->tables[zloc][state][idx];
			zloc = path[j];
			if (state == FSM_UNASSIGNED)
				goto reject;
		}

		return (state);

	reject:
		continue;
	}

	/* UNREACHABLE */
	assert(0);
}

/*
 * Query backmaps[zloc] to find if fsm->tables[zloc][state][i]
 * is a backedge.  Return nonzero if it is, zero otherwise.
 */
static int
isbackedge(struct fsm *fsm, unsigned char *backmaps[TILE_COUNT],
    size_t zloc, unsigned state, size_t i)
{
	ptrdiff_t offset = &fsm->tables[zloc][state][i] - &fsm->tables[zloc][0][0];

	assert(offset >= 0);

	return (backmaps[zloc][offset >> 3] & 1 << (offset & 7));
}

/*
 * Traverse the trie represented by fsm recursively.  traversetrie is
 * called once for for every node in the traversal.  state is the state
 * number we are currently in, path is a record of the path we took from
 * the root to get here and pathlen is the number of vertices in path.
 * fsm->backmaps is used to avoid traversing back edges.  For each edge
 * marked FSM_UNASSIGNED, assignbackedge() is called to assign the edge.
 */
static void
traversetrie(struct fsm *fsm, unsigned char *backmaps[TILE_COUNT], unsigned state,
    char path[SEARCH_PATH_LEN], size_t pathlen)
{
	size_t i, zloc, n_moves;
	const signed char *moves;

	/* make sure we don't go out of bounds */
	assert(pathlen > 0);
	assert(pathlen < SEARCH_PATH_LEN - 1);

	/* ignore special states */
	if (state >= FSM_MAX_LEN)
		return;

	zloc = path[pathlen - 1];
	n_moves = move_count(zloc);
	moves = get_moves(zloc);
	for (i = 0; i < n_moves; i++) {
		path[pathlen] = moves[i];
		if (!isbackedge(fsm, backmaps, zloc, state, i))
			traversetrie(fsm, backmaps, fsm->tables[zloc][state][i], path, pathlen + 1);
		else if (fsm->tables[zloc][state][i] == FSM_UNASSIGNED)
			fsm->tables[zloc][state][i] = longestprefix(fsm, path, pathlen + 1);
	}
}

/*
 * Augment fsm with edges for missed matches (i.e. back edges).
 *
 * The backmaps table contains pointers to bitmaps which store for each
 * transition a 1 if the transition is a back edge.
 */
static void
addbackedges(struct fsm *fsm, int verbose)
{
	unsigned *table;
	size_t i, j;
	unsigned char *backmaps[TILE_COUNT];
	char path[SEARCH_PATH_LEN];

	/* populate backmaps */
	if (verbose)
		fprintf(stderr, "populating backmaps...\n");

	for (i = 0; i < TILE_COUNT; i++) {
		backmaps[i] = calloc((fsm->header.lengths[i] + 1) / 2, 1);
		if (backmaps[i] == NULL) {
			perror("calloc");
			exit(EXIT_FAILURE);
		}

		table = (unsigned *)fsm->tables[i];
		for (j = 0; j < 4 * fsm->header.lengths[i]; j++)
			if (table[j] == FSM_UNASSIGNED)
				backmaps[i][j / 8] |= 1 << j % 8;
	}

	for (i = 0; i < TILE_COUNT; i++) {
		if (verbose)
			fprintf(stderr, "generating back edges for square %2zu\n", i);

		path[0] = i;
		traversetrie(fsm, backmaps, 0, path, 1);
	}

	for (i = 0; i < TILE_COUNT; i++)
		free(backmaps[i]);
}

/*
 * Compute table offsets and write fsm to fsmfile.  Print an error
 * message and exit on failure.  As a side effect, close fsmfile.  This
 * is done to report errors that appear upon fclose.  If verbose is set,
 * print some interesting information to stderr.
 */
static void
writefsm(FILE *fsmfile, struct fsm *fsm, int verbose)
{
	off_t offset, start;
	size_t i, count;

	if (verbose)
		fprintf(stderr, "writing finite state machine...\n");

	start = ftello(fsmfile);

	offset = sizeof fsm->header;
	for (i = 0; i < TILE_COUNT; i++) {
		fsm->header.offsets[i] = offset;
		offset += sizeof *fsm->tables[i] * fsm->header.lengths[i];
	}

	count = fwrite(&fsm->header, sizeof fsm->header, 1, fsmfile);
	if (count != 1) {
		perror("fwrite");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < TILE_COUNT; i++) {
		if (verbose)
			fprintf(stderr, "square %2zu: %10u states (%11zu bytes)\n",
			    i, fsm->header.lengths[i], fsm->header.lengths[i] *
			    sizeof *fsm->tables[i]);

		count = fwrite(fsm->tables[i], sizeof *fsm->tables[i],
		    fsm->header.lengths[i], fsmfile);
		if (count != fsm->header.lengths[i]) {
			perror("fwrite");
			exit(EXIT_FAILURE);
		}
	}

	if (fclose(fsmfile) == EOF) {
		perror("fclose");
		exit(EXIT_FAILURE);
	}

	if (verbose)
		fprintf(stderr, "finite state machine successfully written\n");
}

static void noreturn
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-v] [fsmfile]\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	FILE *fsmfile;
	struct fsm fsm;
	int optchar, verbose = 0;

	while (optchar = getopt(argc, argv, "v"), optchar != EOF)
		switch (optchar) {
		case 'v':
			verbose = 1;
			break;

		default:
			usage(argv[0]);
		}

	switch (argc - optind) {
	case 0:
		fsmfile = stdout;
		break;

	case 1:
		fsmfile = fopen(argv[optind], "wb");
		if (fsmfile == NULL) {
			perror(argv[optind]);
			return (EXIT_FAILURE);
		}

		break;

	default:
		usage(argv[0]);
	}

	initfsm(&fsm);
	readloops(&fsm, stdin);
	addbackedges(&fsm, verbose);
	writefsm(fsmfile, &fsm, verbose);

	return (EXIT_SUCCESS);
}
