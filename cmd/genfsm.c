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

/* genfsm.c -- generate a finite state machine */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "puzzle.h"
#include "compact.h"
#include "search.h"

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s [-l limit] [fsm]\n", argv0);
	exit(EXIT_FAILURE);
}

/*
 * Determine a path leading to configuration cp, the last move of which
 * is last_move.  It is assumed that the path has length len.  rounds is
 * used to look up nodes along the way with the first len entries of
 * rounds being used.
 */
static void
find_path(struct path *path, const struct compact_puzzle *cp, int last_move,
    const struct cp_slice *rounds, size_t len)
{
	/* TODO */
}

/*
 * Search through the latest expansion round and print out all half
 * loops to fsmfile.
 *
 * TODO: Find a way to not print superfluous half-loops.
 */
static void
print_half_loops(FILE *fsmfile, const struct cp_slice *rounds, size_t len)
{
	/* TODO */
}

/*
 * Remove all but the canonical path to entries in cps.
 */
static void
strip_extra_paths(const struct cp_slice *rounds)
{
	/* TODO */
}
