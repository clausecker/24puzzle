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
 * Augment fsm with edges for missed matches (i.e. back edges).
 */
static void
addbackedges(struct fsm *fsm)
{
	/* TODO */
}

/*
 * Compute table offsets and write fsm to fsmfile.  Print an error
 * message and exit on failure.  As a side effect, close fsmfile.  This
 * is done to report errors that appear upon fclose.
 */
static void
writefsm(FILE *fsmfile, struct fsm *fsm)
{
	off_t offset, start;
	size_t i, count;

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
		count = fwrite(fsm->tables[i], sizeof *fsm->tables[i],
		    fsm->header.lengths[i], fsmfile);
		if (count != fsm->header.lengths[i]) {
			perror("fwrite");
			exit(EXIT_FAILURE);
		}
	}

	/* consistency check */
	assert(ftello(fsmfile) == start + offset);

	if (fclose(fsmfile) == EOF) {
		perror("fclose");
		exit(EXIT_FAILURE);
	}
}

static void noreturn
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [fsmfile]\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	FILE *fsmfile;
	struct fsm fsm;

	switch (argc) {
	case 1:
		fsmfile = stdout;
		break;

	case 2:
		fsmfile = fopen(argv[1], "wb");
		if (fsmfile == NULL) {
			perror(argv[1]);
			return (EXIT_FAILURE);
		}

		break;

	default:
		usage(argv[0]);
	}

	initfsm(&fsm);
	readloops(&fsm, stdin);
	addbackedges(&fsm);
	writefsm(fsmfile, &fsm);

	return (EXIT_SUCCESS);
}
