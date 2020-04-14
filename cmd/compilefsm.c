/*-
 * Copyright (c) 2018, 2020 Robert Clausecker. All rights reserved.
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

#include "fsm.h"
#include "puzzle.h"
#include "search.h"

/*
 * Add a new entry to the state table for square sq to fsm.  Resize the
 * underlying array if needed and exit if the state table is full.
 * Return the index of the newly allocated state.
 */
static unsigned
addstate(struct fsm *fsm, struct fsmfile *header, int sq)
{
	unsigned long long newsize;
	unsigned state;

	assert(0 <= sq && sq < TILE_COUNT);
	state = header->lengths[sq]++;
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
initfsm(struct fsm *fsm, struct fsmfile *header)
{
	size_t i;

	memset(fsm, 0, sizeof *fsm);
	memset(header, 0, sizeof *header);

	for (i = 0; i < TILE_COUNT; i++)
		addstate(fsm, header, i);
}

/*
 * Trace the first n steps of p in fsm, adding states as needed.  If
 * any of the states is a match, print an error message and exit.
 * Return the resulting state.
 */
struct fsm_state
tracepath(struct fsm *fsm, struct fsmfile *header, struct path *p, size_t n)
{
	struct fsm_state st;
	size_t i;
	unsigned *dst;
	char pathstr[PATH_STR_LEN];

	st = fsm_start_state(p->moves[0]);
	for (i = 1; i < n; i++) {
		dst = fsm_entry_pointer(fsm, st, p->moves[i]);
		if (*dst == FSM_UNASSIGNED)
			*dst = addstate(fsm, header, p->moves[i]);
		else if (*dst == FSM_MATCH) {
			path_string(pathstr, p);
			fprintf(stderr, "%s: prefix already present: ", pathstr);
			p->pathlen = i + 1; /* kludge */
			path_string(pathstr, p);
			fprintf(stderr, "%s\n", pathstr);
			exit(EXIT_FAILURE);
		}

		/* avoid re-load */
		st.zloc = p->moves[i];
		st.state = *dst;
	}

	return (st);
}

/*
 * Add a half-loop described by path p to the trie in fsm.  Print an
 * error message and exit if a prefix of p is already in fsm.
 */
static void
addloop(struct fsm *fsm, struct fsmfile *header, struct path *p)
{
	struct fsm_state st;
	unsigned *dst;
	char pathstr[PATH_STR_LEN];

	st = tracepath(fsm, header, p, p->pathlen - 1);
	dst = fsm_entry_pointer(fsm, st, p->moves[p->pathlen - 1]);
	if (*dst != FSM_UNASSIGNED) {
		path_string(pathstr, p);
		fprintf(stderr, "%s: is prefix of some other entry\n", pathstr);
		exit(EXIT_FAILURE);
	}

	*dst = FSM_MATCH;
}

/*
 * Make the state for p an alias for newp.  Add entries for newp to the
 * FSM as needed.
 */
static void
addalias(struct fsm *fsm, struct fsmfile *header, struct path *p, struct path *newp)
{
	struct fsm_state st, newst;
	unsigned *dst;
	char pathstr[PATH_STR_LEN];

	st = tracepath(fsm, header, p, p->pathlen - 1);
	newst = tracepath(fsm, header, newp, newp->pathlen);
	dst = fsm_entry_pointer(fsm, st, p->moves[p->pathlen - 1]);
	if (*dst != FSM_UNASSIGNED) {
		path_string(pathstr, p);
		fprintf(stderr, "%s: is prefix of some other entry\n", pathstr);
		exit(EXIT_FAILURE);
	}

	*dst = newst.state;
}

/*
 * Read half loops from loopfile and add them to the pristine FSM
 * structure *fsm.  loopfile contains loops as printed by cmd/genloops.
 * After this function ran, the entries in fsm form a trie containing
 * all the half loops from loopfile.  As an extra invariant, no half
 * loop may be the prefix of another one.  If makealiases is nonzero,
 * loops of the form "A = B" are not eliminated but rather aliased.
 */
static void
readloops(struct fsm *fsm, struct fsmfile *header, FILE *loopfile,
    int makealiases)
{
	struct path p, newp;
	size_t n_linebuf = 0;
	char *linebuf = NULL, *matchstr, *typestr, *replacestr, *saveptr;
	size_t i;
	int lineno = 1;

	while (getline(&linebuf, &n_linebuf, loopfile) > 0) {
		matchstr = strtok_r(linebuf, " \t\n", &saveptr);
		typestr = strtok_r(NULL, " \t\n", &saveptr);

		if (path_parse(&p, matchstr) == NULL) {
			fprintf(stderr, "%s: invalid path on line %d: %s\n",
			    __func__, lineno, matchstr);
			exit(EXIT_FAILURE);
		}

		if (makealiases && *typestr == '=') {
			replacestr = strtok_r(NULL, " \t\n", &saveptr);
			if (path_parse(&newp, replacestr) == NULL) {
				fprintf(stderr, "%s: invalid path on line %d: %s\n",
				    __func__, lineno, replacestr);
				exit(EXIT_FAILURE);
			}

			addalias(fsm, header, &p, &newp);
		} else
			addloop(fsm, header, &p);
		lineno++;
	}

	if (ferror(loopfile)) {
		perror("getline");
		exit(EXIT_FAILURE);
	}

	/* release extra storage */
	for (i = 0; i < TILE_COUNT; i++) {
		assert(header->lengths[i] <= fsm->sizes[i]);

		/* should never fail as we release storage */
		fsm->tables[i] = realloc(fsm->tables[i],
		    header->lengths[i] * sizeof *fsm->tables[i]);
		assert(fsm->tables[i] != NULL);

		fsm->sizes[i] = header->lengths[i];
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
	struct fsm_state st;
	size_t i, j;

	for (i = 0; i < pathlen; i++) {
		st = fsm_start_state(path[i]);
		for (j = i + 1; j < pathlen; j++) {
			/*
			 * there should be no paths of the form abc in
			 * the FSM where b too is in the FSM.  Todo:
			 * better error message.
			 */
			assert(st.state < FSM_MAX_LEN);
			st = fsm_advance(fsm, st, path[j]);
			if (st.state == FSM_UNASSIGNED)
				goto reject;
		}

		return (st.state);

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
    size_t zloc, unsigned *dst)
{
	ptrdiff_t offset = dst - &fsm->tables[zloc][0][0];

	assert(offset >= 0);

	return (backmaps[zloc][offset >> 3] & 1 << (offset & 7));
}

/*
 * Set the backmap entry corresponding to FSM entry dst.
 */
static void
addbackedge(struct fsm *fsm, unsigned char *backmaps[TILE_COUNT],
    size_t zloc, unsigned *dst)
{
	ptrdiff_t offset = dst - &fsm->tables[zloc][0][0];

	assert(offset >= 0);

	backmaps[zloc][offset >> 3] |= 1 << (offset & 7);
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
traversetrie(struct fsm *fsm, unsigned char *backmaps[TILE_COUNT], struct fsm_state st,
    char path[SEARCH_PATH_LEN], size_t pathlen)
{
	struct fsm_state newst;
	size_t i, n_moves;
	unsigned *dst;
	const signed char *moves;

	/* make sure we don't go out of bounds */
	assert(pathlen > 0);
	assert(pathlen < SEARCH_PATH_LEN - 1);

	/* ignore special states */
	if (st.state >= FSM_MAX_LEN)
		return;

	n_moves = move_count(st.zloc);
	moves = get_moves(st.zloc);
	for (i = 0; i < n_moves; i++) {
		path[pathlen] = moves[i];
		dst = fsm_entry_pointer(fsm, st, moves[i]);
		if (!isbackedge(fsm, backmaps, st.zloc, dst)) {
			addbackedge(fsm, backmaps, st.zloc, dst);
			newst.zloc = moves[i];
			newst.state = *dst;
			traversetrie(fsm, backmaps, newst, path, pathlen + 1);
		} else if (*dst == FSM_UNASSIGNED)
			*dst = longestprefix(fsm, path, pathlen + 1);
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
	struct fsm_state st;
	unsigned *table;
	size_t i, j;
	unsigned char *backmaps[TILE_COUNT];
	char path[SEARCH_PATH_LEN];

	/* populate backmaps */
	if (verbose)
		fprintf(stderr, "populating backmaps...\n");

	for (i = 0; i < TILE_COUNT; i++) {
		backmaps[i] = calloc((fsm->sizes[i] + 1) / 2, 1);
		if (backmaps[i] == NULL) {
			perror("calloc");
			exit(EXIT_FAILURE);
		}

		table = (unsigned *)fsm->tables[i];
		for (j = 0; j < 4 * fsm->sizes[i]; j++)
			if (table[j] == FSM_UNASSIGNED)
				backmaps[i][j / 8] |= 1 << j % 8;
	}

	/* add back edges */
	for (i = 0; i < TILE_COUNT; i++) {
		if (verbose)
			fprintf(stderr, "generating back edges for square %2zu\n", i);

		path[0] = i;
		st = fsm_start_state(i);
		traversetrie(fsm, backmaps, st, path, 1);
	}

	/* release backmaps */
	for (i = 0; i < TILE_COUNT; i++)
		free(backmaps[i]);
}

static void noreturn
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-av] [fsmfile]\n", argv0);
	exit(EXIT_FAILURE);
}

extern int
main(int argc, char *argv[])
{
	FILE *fsmfile;
	struct fsm fsm;
	struct fsmfile header;
	int optchar, verbose = 0, makealiases = 0;

	while (optchar = getopt(argc, argv, "av"), optchar != EOF)
		switch (optchar) {
		case 'a':
			makealiases = 1;
			break;

		case 'v':
			verbose = FSM_VERBOSE;
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

	initfsm(&fsm, &header);
	readloops(&fsm, &header, stdin, makealiases);
	addbackedges(&fsm, verbose);
	if (fsm_write(fsmfile, &fsm, verbose) != 0) {
		perror("fsm_write");
		return (EXIT_FAILURE);
	}

	return (EXIT_SUCCESS);
}
