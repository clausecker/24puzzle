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
 * Read half loops from loopfile and add them to the pristine FSM
 * structure *fsm.  loopfile contains loops as printed by cmd/genloops.
 * After this function ran, the entries in fsm form a trie containing
 * all the half loops from loopfile.  As an extra invariant, no half
 * loop may be the prefix of another one.
 */
static void
readloops(struct fsm *fsm, FILE *loopfile)
{
	/* TODO */
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

	memset(&fsm, 0, sizeof fsm);
	readloops(&fsm, stdin);
	addbackedges(&fsm);
	writefsm(fsmfile, &fsm);

	return (EXIT_SUCCESS);
}
