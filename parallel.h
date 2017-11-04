/*-
 * Copyright (c) 2017 Robert Clausecker. All rights reserved.
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

#ifndef PARALLEL_H
#define PARALLEL_H

#include "pdb.h"

/*
 * This file contains some helper functions for parallel iteration
 * through endgame tablebases.  The function pdb_iterate_parallel()
 * uses pdb_jobs threads to iterate through the PDB, calling the worker
 * function for each maprank.  The worker function can submit results
 * by embedding struct parallel_config at the beginning of a custom
 * structure like this:
 *
 *     struct my_config {
 *         struct parallel_config pcfg;
 *         _Atomic int widget_count;
 *         ...
 *     }
 */
struct parallel_config {
	struct patterndb *pdb;
	_Atomic tsrank nextrank;	/* start of next chunk to be done */

	/* worker function */
	void (*worker)(void *, struct index *);
};

extern void pdb_iterate_parallel(struct parallel_config *);

#endif /* PARALLEL_H */
