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

/* fsm.h -- finite state machines */

#ifndef FSM_H
#define FSM_H

#include <stdlib.h>
#include <stdio.h>

#include "builtins.h"
#include "puzzle.h"

/*
 * The header of a finite state machine file.  A finite state machine
 * file first contains this header and is followed by TILE_COUNT state
 * tables, one for each state.  The offsets member stores the beginnings
 * of these tables, lengths stores the number of 32 bit integers in each
 * table.  struct fsm is the in-memory representation of a finite state
 * machine and contains pointers to the tables as well as the actually
 * allocated table sizes.
 */
struct fsmfile {
	/* table offsets in bytes from the beginning of the file */
	off_t offsets[TILE_COUNT];

	/* number of table entries */
	unsigned lengths[TILE_COUNT];
};

/*
 * An FSM file can be augmented with a set of moribund state tables.  If
 * present, pointers to them follow the main header.  If all state
 * tables have offsets to after this extended header, we assume that it
 * is present.
 */
struct fsmfile_moribund {
	/* the normal header */
	struct fsmfile header;

	/* offsets of the moribund state tables */
	off_t moribund_offsets[TILE_COUNT];
};

struct fsm {
	unsigned sizes[TILE_COUNT];
	unsigned (*tables[TILE_COUNT])[4];
	unsigned char *moribund[TILE_COUNT];
};

/*
 * A state in a finite state machine.  The state is composed of the
 * zero tile's location and the offset in the corresponding table.
 * Objects of this type are always treated as values.
 */
struct fsm_state {
	unsigned zloc, state;
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
#define FSM_NEW_DEAD   0xfffffff1u /* for internal use by test/fsmdeadends.c */
#define FSM_DEAD_END   0xfffffff2u /* for internal use by test/fsmdeadends.c */
#define FSM_MATCH      0xfffffffeu
#define FSM_UNASSIGNED 0xffffffffu

/*
 * flags for fsm_write
 */
enum {
	FSM_VERBOSE  = 1 << 0, /* be verbose */
	FSM_MORIBUND = 1 << 1, /* write moribund state tables */
};

extern struct fsm	*fsm_load(FILE *);
extern int		 fsm_write(FILE *, const struct fsm *, int);
extern int		 fsm_get_moves(signed char[static 4], struct fsm_state, const struct fsm *);

extern const struct fsm fsm_dummy, fsm_simple;

/*
 * Release storage associated with finite state machine fsm.
 */
static inline void
fsm_free(struct fsm *fsm)
{
	size_t i;

	for (i = 0; i < TILE_COUNT; i++) {
		free(fsm->tables[i]);
		free(fsm->moribund[i]);
	}

	free(fsm);
}

/*
 * Enter the initial state for the given zero tile location.
 */
static inline struct fsm_state
fsm_start_state(size_t zloc)
{
	struct fsm_state st;

	st.zloc = zloc;
	st.state = FSM_BEGIN;

	return (st);
}

/*
 * Are we in a matching state right now?
 */
static inline int
fsm_is_match(struct fsm_state st)
{
	return (st.state == FSM_MATCH);
}

/*
 * Get the moribundness number of the current state.
 * This number is 0 if the current state is a match.
 */
static inline int
fsm_moribundness(const struct fsm *fsm, struct fsm_state st)
{
	if (fsm_is_match(st))
		return (0);
	else
		return (fsm->moribund[st.zloc][st.state]);
}

/*
 * Given an FSM, a state, and the zero tile location of the destination,
 * return a pointer to the corresponding table entry.
 */
static inline unsigned *
fsm_entry_pointer(const struct fsm *fsm, struct fsm_state st, size_t newzloc)
{
	return (&fsm->tables[st.zloc][st.state][move_index(st.zloc, newzloc)]);
}

/*
 * Given an FSM and a state, prefetch the outgoing transitions for that
 * state.
 */
static inline void
fsm_prefetch(const struct fsm *fsm, struct fsm_state st)
{
	prefetch(fsm->tables[st.zloc][st.state]);
}

/*
 * Advance st by moving to newzloc.
 */
static inline struct fsm_state
fsm_advance(const struct fsm *fsm, struct fsm_state st, size_t newzloc)
{
	st.state = *fsm_entry_pointer(fsm, st, newzloc);
	st.zloc = newzloc;

	return (st);
}

/*
 * Advance st by moving to move index i as given by get_moves().
 * This performs better than fsm_advance if i is known.
 */
static inline struct fsm_state
fsm_advance_idx(const struct fsm *fsm, struct fsm_state st, size_t i)
{
	st.state = fsm->tables[st.zloc][st.state][i];
	st.zloc = get_moves(st.zloc)[i];

	return (st);
}

#endif /* FSM_H */
