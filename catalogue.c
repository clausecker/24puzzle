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

/* catalogue.c -- pattern database catalogues */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "builtins.h"
#include "catalogue.h"
#include "pdb.h"
#include "puzzle.h"
#include "tileset.h"
#include "heuristic.h"

enum { LINEBUF_LEN = 512 };

/*
 * Add a PDB for the tile set represented by string tsbuf to the last
 * heuristic in cat.  If the PDB is not already present, load or
 * generate it, possibly generatic files in pdbdir.  Print status
 * information to f if f is not NULL.  If f&CAT_IDENTIFY, identify PDB
 * entries on load and build.  On success return the index of the PDB
 * loaded, on error set errno and return -1.
 */
static int
add_pdb(struct pdb_catalogue *cat, const char *tsbuf, const char *pdbdir,
    int flags, int heuflags, FILE *f)
{
	size_t pdbidx;
	tileset ts;
	const char *heutype = "pdb";

	if (tileset_parse(&ts, tsbuf) != 0) {
		if (f != NULL)
			fprintf(f, "Cannot parse tileset: %s\n", tsbuf);

		errno = EINVAL;
		return (-1);
	}

	if (tileset_has(ts, ZERO_TILE)) {
		heutype = flags & CAT_IDENTIFY ? "ipdb" : "zpdb";
		ts = tileset_remove(ts, ZERO_TILE);
	}

	/* TODO: Replace f with a verbose flag */
	if (f != NULL)
		heuflags |= HEU_VERBOSE;

	/* check if the PDB is already present */
	for (pdbidx = 0; pdbidx < cat->n_heus; pdbidx++)
		if (cat->pdbs_ts[pdbidx] == ts)
			return (pdbidx);


	/* if the PDB is not already present, allocate it */
	pdbidx = cat->n_heus++;
	if (pdbidx >= CATALOGUE_HEUS_LEN) {
		if (f != NULL)
			fprintf(f, "Too many PDBs, up to %d are possible.\n",
			    CATALOGUE_HEUS_LEN);

		errno = ERANGE;
		return (-1);
	}

	cat->pdbs_ts[pdbidx] = ts;
	if (heu_open(cat->heus + pdbidx, pdbdir, ts, heutype, heuflags) != 0)
		return (-1);

	return (pdbidx);
}

/*
 * Load a catalogue from catfile, if pdbdir is not NULL, search for PDBs
 * in pdbdir. Generate missing PDBs and store them in pdbdir if pdbdir
 * is not NULL.  Print status information to f if f is not NULL.
 *
 * A catalogue file contains groups of tilesets.  Each group forms one
 * heuristic.  The tileset name is used to form a file name for the PDB,
 * so the order of components should be the same every time.  If the
 * same PDB is used in multiple heuristics, it is loaded only once
 * still.  For better performance, the PDB is loaded as a memory mapped
 * file.  On error, NULL is returned and errno set to indicate the
 * problem.
 */
extern struct pdb_catalogue *
catalogue_load(const char *catfile, const char *pdbdir, int flags, FILE *f)
{
	struct pdb_catalogue *cat = malloc(sizeof *cat);
	FILE *catcfg;
	size_t i;
	int error, pdbidx;
	tileset ctiles = EMPTY_TILESET;
	char linebuf[LINEBUF_LEN], *newline;

	if (cat == NULL)
		return (NULL);

	memset(cat, 0, sizeof *cat);

	if (f != NULL)
		fprintf(f, "Loading PDB catalogue from %s\n", catfile);

	catcfg = fopen(catfile, "r");
	if (catcfg == NULL) {
		error = errno;
		if (f != NULL)
			fprintf(f, "%s: %s\n", catfile, strerror(errno));

		goto earlyfail;
	}

	while (fgets(linebuf, sizeof linebuf, catcfg) != NULL) {
		newline = strchr(linebuf, '\n');
		if (newline == NULL) {
			if (f != NULL)
				fprintf(stderr, "Overlong line or file doesn't end in a newline: %s\n", catfile);

			error = ERANGE;
			goto fail;
		}

		*newline = '\0';

		/* skip comments */
		if (linebuf[0] == '#')
			continue;

		/* empty lines demark groups of PDBs forming a heuristic */
		if (linebuf[0] == '\0') {
			if (!tileset_empty(ctiles)) {
				if (tileset_add(ctiles, ZERO_TILE) != FULL_TILESET && f != NULL)
					fprintf(f, "Warning: heuristic %zu does not account for all tiles!\n",
					    cat->n_heuristics);

				cat->n_heuristics++;
			}

			ctiles = EMPTY_TILESET;
			continue;
		}

		if (cat->n_heuristics >= HEURISTICS_LEN) {
			if (f != NULL)
				fprintf(f, "Too many heuristics, up to %d are possible.\n",
				    HEURISTICS_LEN);

			error = ERANGE;
			goto fail;
		}

		pdbidx = add_pdb(cat, linebuf, pdbdir, flags, HEU_CREATE | HEU_SIMILAR, f);
		if (pdbidx == -1) {
			error = errno;
			goto fail;
		}

		if (!tileset_empty(tileset_remove(tileset_intersect(ctiles,
		    cat->pdbs_ts[pdbidx]), ZERO_TILE)) && f != NULL)
			fprintf(f, "Warning: heuristic %zu not admissible!\n", cat->n_heuristics);

		ctiles = tileset_union(ctiles, cat->pdbs_ts[pdbidx]);

		cat->parts[cat->n_heuristics] |= 1 << pdbidx;
	}

	if (ferror(catcfg)) {
		error = errno;
		if (f != NULL)
			fprintf(f, "%s: %s\n", catfile, strerror(errno));

		goto fail;
	}

	/* in lieu of an empty line at EOF */
	if (ctiles != EMPTY_TILESET) {
		if (tileset_add(ctiles, ZERO_TILE) != FULL_TILESET && f != NULL)
			fprintf(f, "Warning: heuristic %zu does not account for all tiles!\n",
			    cat->n_heuristics);

		cat->n_heuristics++;
	}

	if (f != NULL)
		fprintf(f, "Loaded %zu PDBs and %zu heuristics from %s\n",
		    cat->n_heus, cat->n_heuristics, catfile);

	fclose(catcfg);

	return (cat);

fail:
	fclose(catcfg);
	for (i = 0; i < cat->n_heus; i++)
		heu_free(cat->heus + i);

earlyfail:
	free(cat);

	errno = error;
	return (NULL);
}

/*
 * Release store associated with PDB catalogue cat.  Also release
 * storage associated with all PDBs we opened.  *cat is undefined
 * after this function returned and must not be used further.
 */
extern void
catalogue_free(struct pdb_catalogue *cat)
{
	size_t i;

	for (i = 0; i < cat->n_heus; i++)
		heu_free(cat->heus + i);

	free(cat);
}

/*
 * Amend a PDB catalogue to also include transposed PDBs.
 */

/*
 * Fill in a struct partial_hvals with values for puzzle configuration p
 * relative to PDB catalogue cat.
 */
extern void
catalogue_partial_hvals(struct partial_hvals *ph,
    struct pdb_catalogue *cat, const struct puzzle *p)
{
	size_t i;

	for (i = 0; i < cat->n_heus; i++)
		ph->hvals[i] = heu_hval(cat->heus + i, p);
}

/*
 * Update ph, a struct partial_hvals for a configuration neighboring p
 * by moving tile t, to contain partial h values for p.  To save time,
 * we only look up those PDB entries that changed when moving tile.
 */
extern void
catalogue_diff_hvals(struct partial_hvals *ph, struct pdb_catalogue *cat,
    const struct puzzle *p, unsigned tile)
{
	size_t i;

	for (i = 0; i < cat->n_heus; i++)
		if (tileset_has(cat->pdbs_ts[i], tile))
			ph->hvals[i] = heu_diff_hval(cat->heus + i, p, ph->hvals[i]);
}

/*
 * Update cat to include for each heuristic the appropriate transposed
 * heuristic.  Return 0 on success, -1 on failure.  On error, print
 * diagnostic messages to f if f != NULL.  On failure, the content of
 * cat is unchanged.
 */
extern int
catalogue_add_transpositions(struct pdb_catalogue *cat)
{
	struct pdb_catalogue newcat = *cat;
	unsigned long long set, newset;
	size_t transpositions[CATALOGUE_HEUS_LEN];
	size_t i, j, n_heus, n_heuristics;
	tileset ts;

	/* first, add a transposed heuristic for each heuristic */
	n_heus = newcat.n_heus;
	for (i = 0; i < n_heus; i++) {
		ts = tileset_transpose(newcat.pdbs_ts[i]);

		/* do we already have this one? */
		for (j = 0; j < newcat.n_heus; j++)
			if (newcat.pdbs_ts[j] == ts) {
				transpositions[i] = j;
				goto continue_outer1;
			}

		/* no transposition found, must create */
		if (newcat.n_heus >= CATALOGUE_HEUS_LEN) {
			errno = ENOMEM;
			return (-1);
		}

		heu_morph(newcat.heus + newcat.n_heus, newcat.heus + i, 4);
		assert(newcat.heus[newcat.n_heus].ts == ts);
		newcat.pdbs_ts[newcat.n_heus] = newcat.heus[newcat.n_heus].ts;
		transpositions[i] = newcat.n_heus++;

	continue_outer1:
		;
	}

	/* next, construct transposed heuristics */
	n_heuristics = newcat.n_heuristics;
	for (i = 0; i < n_heuristics; i++) {
		/* process bits one by one */
		newset = 0;
		for (set = newcat.parts[i]; set != 0; set &= set - 1)
			newset |= transpositions[ctzll(set)];

		/* do we already have this one? */
		for (j = 0; j < newcat.n_heuristics; j++)
			if (newcat.parts[j] == newset)
				goto continue_outer2;

		/* have we exceeded the available storage? */
		if (newcat.n_heuristics >= HEURISTICS_LEN) {
			errno = ENOMEM;
			return (-1);
		}

		newcat.parts[newcat.n_heuristics++] = newset;

	continue_outer2:
		;
	}

	*cat = newcat;

	return (0);
}
