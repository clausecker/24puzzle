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

/* fsm.c -- finite state machines */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fsm.h"
#include "puzzle.h"

/*
 * fsm_dummy is a finite state machine which does not recognize any
 * patterns at all.
 */
static const unsigned dummy_states[1][4] = {
	FSM_BEGIN, FSM_BEGIN, FSM_BEGIN, FSM_BEGIN,
};

const struct fsm fsm_dummy = {
	.sizes = {
		1, 1, 1, 1, 1,
		1, 1, 1, 1, 1,
		1, 1, 1, 1, 1,
		1, 1, 1, 1, 1,
		1, 1, 1, 1, 1,
	},
#define DUMMY (unsigned (*)[4])dummy_states
	.tables = {
		DUMMY, DUMMY, DUMMY, DUMMY, DUMMY,
		DUMMY, DUMMY, DUMMY, DUMMY, DUMMY,
		DUMMY, DUMMY, DUMMY, DUMMY, DUMMY,
		DUMMY, DUMMY, DUMMY, DUMMY, DUMMY,
		DUMMY, DUMMY, DUMMY, DUMMY, DUMMY,
	},
#undef DUMMY
};

/*
 * fsm_simple is a finite state machine that just recognizes loops of
 * length 2, i.e. moves that directly undo the previous move.  To save
 * space, state arrays are reused where possible.
 */
static const unsigned simple_states_LU[5][4] = {
	             2,              1, FSM_UNASSIGNED, FSM_UNASSIGNED, /* begin */
	FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from above (dummy) */
	FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from left (dummy) */
	     FSM_MATCH,              1, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from right */
	             2,      FSM_MATCH, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from below */
};

static const unsigned simple_states_U[5][4] = {
	             3,              2,              1, FSM_UNASSIGNED, /* begin */
	FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from above (dummy) */
	     FSM_MATCH,              2,              1, FSM_UNASSIGNED, /* from left */
	             3,      FSM_MATCH,              1, FSM_UNASSIGNED, /* from right */
	             3,              2,      FSM_MATCH, FSM_UNASSIGNED, /* from below */
};

static const unsigned simple_states_RU[5][4] = {
	             3,              1, FSM_UNASSIGNED, FSM_UNASSIGNED, /* begin */
	FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from above (dummy) */
	     FSM_MATCH,              1, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from left */
	FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from right (dummy) */
	             3,      FSM_MATCH, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from below */
};

/* same entries where not FSM_UNASSIGNED */
#define simple_states_LD simple_states_L
static const unsigned simple_states_L[5][4] = {
	             4,              2,              1, FSM_UNASSIGNED, /* begin */
	     FSM_MATCH,              2,              1, FSM_UNASSIGNED, /* from above */
	FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from left (dummy) */
	             4,      FSM_MATCH,              1, FSM_UNASSIGNED, /* from right */
	             4,              2,      FSM_MATCH, FSM_UNASSIGNED, /* from below */
};

/* same entries where not FSM_UNASSIGNED */
#define simple_states_D simple_states_C
#define simple_states_RD simple_states_C
static const unsigned simple_states_C[5][4] = {
	             4,              3,              2,              1, /* begin */
	     FSM_MATCH,              3,              2,              1, /* from above */
	             4,      FSM_MATCH,              2,              1, /* from left */
	             4,              3,      FSM_MATCH,              1, /* from right */
	             4,              3,              2,      FSM_MATCH, /* from below */
};

static const unsigned simple_states_R[5][4] = {
	             4,              3,              1, FSM_UNASSIGNED, /* begin */
	     FSM_MATCH,              3,              1, FSM_UNASSIGNED, /* from above */
	             4,      FSM_MATCH,              1, FSM_UNASSIGNED, /* from left */
	FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, FSM_UNASSIGNED, /* from right (dummy) */
	             4,              3,      FSM_MATCH, FSM_UNASSIGNED, /* from below */
};

const struct fsm fsm_simple = {
	.sizes = {
		5, 5, 5, 5, 5,
		5, 5, 5, 5, 5,
		5, 5, 5, 5, 5,
		5, 5, 5, 5, 5,
		5, 5, 5, 5, 5,
	},
#define STATES(xy) (unsigned (*)[4])simple_states_##xy
	.tables = {
		STATES(LU), STATES(U), STATES(U), STATES(U), STATES(RU),
		STATES(L), STATES(C), STATES(C), STATES(C), STATES(R),
		STATES(L), STATES(C), STATES(C), STATES(C), STATES(R),
		STATES(L), STATES(C), STATES(C), STATES(C), STATES(R),
		STATES(LD), STATES(D), STATES(D), STATES(D), STATES(RD),
	},
#undef STATES
#undef simple_states_D
#undef simple_states_RD
#undef simple_states_LD
};

/*
 * Check if moribund tables are present.  If none of the tables lie
 * inside the suspected extended header, we assume that they are.
 */
static int
has_moribund(struct fsmfile *header)
{
	size_t i;

	for (i = 0; i < TILE_COUNT; i++)
		if (header->offsets[i] < sizeof (struct fsmfile_moribund))
			return (0);

	return (1);
}

/*
 * Load a finite state machine from file fsmfile.  On success, return a
 * pointer to the FSM loader.  On error, return NULL and set errno to
 * indicate the problem.
 */
extern struct fsm *
fsm_load(FILE *fsmfile)
{
	struct fsmfile_moribund header;
	struct fsm *fsm = malloc(sizeof *fsm);
	size_t i, count;
	int error, moribund;

	if (fsm == NULL)
		return (NULL);

	/* make it easier to bail out */
	for (i = 0; i < TILE_COUNT; i++) {
		fsm->tables[i] = NULL;
		fsm->moribund[i] = NULL;
	}

	/* load main header */
	rewind(fsmfile);
	count = fread(&header.header, sizeof header.header, 1, fsmfile);
	if (count != 1)
		goto fail_ferror;

	/* load extended header if present */
	moribund = has_moribund(&header.header);
	if (moribund) {
		/* fetch remaining header */
		rewind(fsmfile);
		if (ferror(fsmfile))
			goto fail;

		count = fread(&header, sizeof header, 1, fsmfile);
		if (count != 1)
			goto fail_ferror;
	}

	/* load tables */
	for (i = 0; i < TILE_COUNT; i++) {
		fsm->sizes[i] = header.header.lengths[i];
		fsm->tables[i] = malloc(fsm->sizes[i] * sizeof *fsm->tables[i]);
		if (fsm->tables[i] == NULL)
			goto fail;

		if (fseeko(fsmfile, header.header.offsets[i], SEEK_SET) != 0)
			goto fail;

		count = fread(fsm->tables[i], sizeof *fsm->tables[i], fsm->sizes[i], fsmfile);
		if (count != fsm->sizes[i])
			goto fail_ferror;
	}

	/* load moribund tables or fake them */
	for (i = 0; i < TILE_COUNT; i++) {
		fsm->moribund[i] = malloc(fsm->sizes[i] * sizeof *fsm->moribund[i]);
		if (fsm->moribund[i] == NULL)
			goto fail;

		if (moribund) {
			if (fseeko(fsmfile, header.moribund_offsets[i], SEEK_SET) != 0)
				goto fail;

			count = fread(fsm->moribund[i], sizeof *fsm->moribund[i], fsm->sizes[i], fsmfile);
			if (count != fsm->sizes[i])
				goto fail_ferror;
		} else
			memset(fsm->moribund[i], -1, fsm->sizes[i] * sizeof *fsm->moribund[i]);
	}

	return (fsm);

fail_ferror:
	/* handle EOF condition */
	if (!ferror(fsmfile))
		errno = EINVAL;

fail:
	error = errno;
	fsm_free(fsm);
	errno = error;

	return (NULL);
}

/*
 * Fill moves with a list of moves possible from the zero tile location
 * in st that are allowed under fsm.  Return the number of moves.
 * Like with get_moves(), the remaining bytes of moves are filled with
 * -1.
 */
extern int
fsm_get_moves(signed char moves[static 4], struct fsm_state st,
    const struct fsm *fsm)
{
	int n, i;
	const signed char *fullmoves;

	memset(moves, -1, 4);

	fullmoves = get_moves(st.zloc);
	for (n = i = 0; i < move_count(st.zloc); i++)
		if (fsm->tables[st.zloc][st.state][i] != FSM_MATCH)
			moves[n++] = fullmoves[i];

	return (n);
}
