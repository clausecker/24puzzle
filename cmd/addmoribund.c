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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "puzzle.h"
#include "fsm.h"

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-nv] [input.fsm [output.fsm]]\n", argv0);

	exit(EXIT_FAILURE);
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

	fsm_add_moribund(fsm, verbose);

	if (!no_write && fsm_write(outfile, fsm, verbose | FSM_MORIBUND) != 0) {
		perror("fsm_write");
		return (EXIT_FAILURE);
	}


	return (EXIT_SUCCESS);
}
