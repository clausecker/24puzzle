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

/* addmoribund.c -- add moribund state tables to an existing FSM */

#define _POSIX_C_SOURCE 200809L
#include <puzzle.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fsm.h"

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-nv] [input.fsm [output.fsm]]\n", argv0);

	exit(EXIT_FAILURE);
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
static void
add_moribund_states(struct fsm *fsm, int verbose)
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

extern int
main(int argc, char *argv[])
{
	struct fsm *fsm;
	FILE *infile = stdin, *outfile = stdout;
	int optchar, verbose = 0, no_write = 0;

	while (optchar = getopt(argc, argv, "nv"), optchar != -1)
		switch (optchar) {
		case 'n':
			no_write = 1;
			break;

		case 'v':
			verbose = FSM_VERBOSE;
			break;

		default:
			usage(argv[0]);
		}

	switch (argc - optind) {
	case 2:	outfile = fopen(argv[optind + 1], "wb");
		if (outfile == NULL) {
			perror(argv[optind + 1]);
			return (EXIT_FAILURE);
		}

		/* FALLTHROUGH */
	case 1:	infile = fopen(argv[optind], "rb");
		if (infile == NULL) {
			perror(argv[optind]);
			return (EXIT_FAILURE);
		}

		/* FALLTHROUGH */
	case 0:	break;

	default:
		usage(argv[0]);
	}

	if (!no_write && argc - optind < 2 && isatty(fileno(stdout))) {
		fprintf(stderr, "will not write state machine to your terminal\n");
		no_write = 1;
	}

	if (verbose)
		fprintf(stderr, "loading finite state machine...\n");

	fsm = fsm_load(infile);
	if (fsm == NULL) {
		perror("fsm_load");
		exit(EXIT_FAILURE);
	}

	add_moribund_states(fsm, verbose);

	if (!no_write && fsm_write(outfile, fsm, verbose | FSM_MORIBUND) != 0) {
		perror("fsm_write");
		return (EXIT_FAILURE);
	}


	return (EXIT_SUCCESS);
}
