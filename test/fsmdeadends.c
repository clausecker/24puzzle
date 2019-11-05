/*-
 * Copyright (c) 2019 Robert Clausecker. All rights reserved.
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

/* fsmdeadends.c -- count how many inescapeable configurations an FSM has */

#include <stdlib.h>
#include <stdio.h>
#include "fsm.h"

/*
 * return nonzero if table entry i for table t in fsm is a dead end.
 * A table entry is a dead end if all outgoing transitions are matches.
 */
static int
is_dead_end(struct fsm *fsm, int t, size_t i)
{
	size_t j;

	if (t < 0 || i >= FSM_DEAD_END)
		return (1);

	for (j = 0; j < 4; j++) {
		if (fsm->tables[t][i][j] < FSM_DEAD_END)
			break;
	}

	return (j == 4);
}

/*
 * find out how many FSM states for tile t accept all outgoing
 * transitions (i.e. are dead ends).  Return that number.
 */
static unsigned long long
count_dead_ends(struct fsm *fsm, int t)
{
	unsigned long long count = 0;
	size_t i;

	for (i = 0; i < fsm->sizes[t]; i++)
		if (is_dead_end(fsm, t, i))
			count++;

	return (count);
}

/*
 * turn all configurations that lead to nothing but dead ends into
 * dead ends themselves.  This function works in two passes: first,
 * all new dead ends are detected.  Then, these new found dead ends
 * are turned into proper dead ends.  This avoids detecting dead
 * ends earlier than expected.
 */
static void
transitive_closure(struct fsm *fsm)
{
	int t;
	size_t i, j;

	/* phase one: detect new dead ends */
	for (t = 0; t < TILE_COUNT; t++)
		for (i = 0; i < fsm->sizes[t]; i++) {
			for (j = 0; j < 4; j++) {
				if (!is_dead_end(fsm, get_moves(t)[j], fsm->tables[t][i][j]))
					break;
			}

			/* if not all outgoing edges lead to dead ends, continue */
			if (j < 4)
				continue;

			/* found a new dead end */
			for (j = 0; j < 4; j++)
				fsm->tables[t][i][j] = FSM_NEW_DEAD;

		}

	/* phase two: turn new dead ends into proper dead ends */
	for (t = 0; t < TILE_COUNT; t++)
		for (i = 0; i < fsm->sizes[t]; i++)
			for (j = 0; j < 4; j++)
				if (fsm->tables[t][i][j] == FSM_NEW_DEAD)
					fsm->tables[t][i][j] = FSM_DEAD_END;
}

extern int
main(int argc, char *argv[])
{
	unsigned long long prev, count, total;
	int i, j;
	struct fsm *fsm;
	FILE *fsmfile;

	if (argc != 2) {
		fprintf(stderr, "usage: %s fsmfile\n", argv[0]);
		return (EXIT_FAILURE);
	}

	fsmfile = fopen(argv[1], "rb");
	if (fsmfile == NULL) {
		perror(argv[1]);
		return (EXIT_FAILURE);
	}

	fsm = fsm_load(fsmfile);
	if (fsm == NULL) {
		perror("fsm_load");
		return (EXIT_FAILURE);
	}

	fclose(fsmfile);

	i = 0;
	total = 0;
	do {
		prev = total;
		total = 0;
		for (j = 0; j < TILE_COUNT; j++) {
			count = count_dead_ends(fsm, j);
			total += count;
			printf("%5d/%d  %20llu\n", j, i, count);
		}

		printf("total/%d  %20llu\n", i++, total);
		transitive_closure(fsm);
	} while (prev < total);

	return (EXIT_SUCCESS);
}
