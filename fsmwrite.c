/*-
 * Copyright (c) 2020 Robert Clausecker. All rights reserved.
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

/* fsmwrite.c -- writing finite state machines to disk */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>

#include "fsm.h"
#include "puzzle.h"

/*
 * Compute table offsets and write fsm to fsmfile.  Set errno and return
 * -1 on failure.  As a side effect, fsmfile may be closed.  This
 * is done to report errors that appear upon fclose.  If FSM_VERBOSE is
 * set in flags, print some interesting information to stderr.  If
 * FSM_MORIBUND is set in flags, write moribund state tables, too.
 */
extern int
fsm_write(FILE *fsmfile, const struct fsm *fsm, int flags)
{
	struct fsmfile_moribund header;
	off_t offset;
	size_t headerlen, i, count;

	if (flags & FSM_VERBOSE)
		fprintf(stderr, "writing finite state machine...\n");

	/* populate the header */
	headerlen = flags & FSM_MORIBUND ? sizeof header : sizeof header.header;
	offset = (off_t)headerlen;
	for (i = 0; i < TILE_COUNT; i++) {
		header.header.offsets[i] = offset;
		header.header.lengths[i] = fsm->sizes[i];
		offset += sizeof *fsm->tables[i] * fsm->sizes[i];
	}

	if (flags & FSM_MORIBUND)
		for (i = 0; i < TILE_COUNT; i++) {
			header.moribund_offsets[i] = offset;
			offset += sizeof *fsm->moribund[i] * fsm->sizes[i];
		}

	count = fwrite(&header, headerlen, 1, fsmfile);
	if (count != 1)
		return (-1);

	for (i = 0; i < TILE_COUNT; i++) {
		if (flags & FSM_VERBOSE)
			fprintf(stderr, "square %2zu: %10u states (%11zu bytes)\n",
			    i, fsm->sizes[i], fsm->sizes[i] * sizeof *fsm->tables[i]);

		count = fwrite(fsm->tables[i], sizeof *fsm->tables[i],
		    fsm->sizes[i], fsmfile);
		if (count != fsm->sizes[i])
			return (-1);
	}

	if (flags & FSM_MORIBUND) {
		if (flags & FSM_VERBOSE)
			fprintf(stderr, "writing moribund state tables...\n");

		for (i = 0; i < TILE_COUNT; i++) {
			count = fwrite(fsm->moribund[i], sizeof *fsm->moribund[i],
			    fsm->sizes[i], fsmfile);
			if (count != fsm->sizes[i])
			    return (-1);
		}
	}

	if (fclose(fsmfile) == EOF)
		return (-1);

	if (flags & FSM_VERBOSE)
		fprintf(stderr, "finite state machine successfully written\n");

	return (0);
}

/*
 * Compute the moribundness number state st should have.  For
 * an accepting state, this number is 0.  For all other states, this
 * number is one higher than the highest moribundness number of all
 * states this state leads to.  Numbers larger than 0xff return 0xff.
 */
static int
moribundness_number(struct fsm *fsm, struct fsm_state st)
{
	struct fsm_state nst;
	int m, m_i, i = 0;

	if (fsm_is_match(st))
		return (0);

	for (i = 0; i < move_count(st.zloc); i++) {
		nst = fsm_advance_idx(fsm, st, i);
		m_i = 1 + fsm_moribundness(fsm, nst);
		if (m_i > m)
			m = m_i; 
	}

	if (m > 0xff)
		m = 0xff;

	return (m);
}

/*
 * Fill in the moribund state tables of fsm.  Initially, fsm_load fills
 * all tables with 0xff, a value hopefully larger than the maximal path
 * length.  To generate the moribund states, we proceed similar to the
 * code in pdbgen.c: in each iteration, we mark all states that only
 * lead to states of moribundness at most k as having moribundnes k+1.
 * This is iterated until changes subside.
 */
extern void
fsm_add_moribund(struct fsm *fsm, int verbose)
{
	struct fsm_state st;
	size_t count, total, size;
	double scale;
	int i, round, m;

	if (verbose) {
		fprintf(stderr, "adding moribund states...\n");

		size = 0;
		for (i = 0; i < TILE_COUNT; i++)
			size += fsm->sizes[i];

		scale = 100.0 / size;
	}

	round = 1;
	total = 0;
	do {
		count = 0;
		for (st.zloc = 0; st.zloc < TILE_COUNT; st.zloc++)
			for (st.state = 0; st.state < fsm->sizes[st.zloc]; st.state++) {
				m = moribundness_number(fsm, st);
				if (m == round) {
					fsm->moribund[st.zloc][st.state] = m;
					count++;
				}
			}

		if (verbose)
			fprintf(stderr, "%5d: %20zu (%5.2f%%)\n", round, count, count * scale);

		total += count;
		round++;
	} while (count > 0);

	if (verbose) {
		fprintf(stderr, "total: %20zu (%5.2f%%)\n", total, total * scale);

		/* count non-moribund states */
		count++;
		for (st.zloc = 0; st.zloc < TILE_COUNT; st.zloc++)
			for (st.state = 0; st.state < fsm->sizes[st.zloc]; st.state++)
				if (fsm->moribund[st.zloc][st.state] == 0xff)
					count++;

		fprintf(stderr, "other: %20zu (%5.2f%%)\n", count, count * scale);
	}
}
