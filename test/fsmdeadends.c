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
 * find out how many FSM states for tile t accept all outgoing
 * transitions (i.e. are dead ends).  Return that number.
 */
static unsigned long long
count_dead_ends(struct fsm *fsm, int t)
{
	unsigned long long count = 0;
	size_t i, j;
	const unsigned (*table)[4];

	table = fsm->tables[t];
	for (i = 0; i < fsm->sizes[t]; i++) {
		for (j = 0; j < 4; j++) {
			if (table[i][j] <= FSM_MAX_LEN)
				break;
		}

		/* if all were FSM_MATCH or FSM_UNASSIGNED */
		if (j == 4)
			count++;
	}

	return (count);
}

extern int
main(int argc, char *argv[])
{
	unsigned long long count, total;
	int i;
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

	total = 0;
	for (i = 0; i < TILE_COUNT; i++) {
		count = count_dead_ends(fsm, i);
		total += count;
		printf("%5d  %20llu\n", i, count);
	}

	printf("total  %20llu\n", total);

	return (EXIT_SUCCESS);
}
