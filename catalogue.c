/* catalogue.c -- pattern database catalogues */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "catalogue.h"
#include "pdb.h"
#include "puzzle.h"
#include "tileset.h"

enum { LINEBUF_LEN = 512 };

/*
 * Add a PDB for the tile set represented by string tsbuf to the last
 * heuristic in cat.  If the PDB is not already present, load or
 * generate it, possibly generatic files in pdbdir.  Print status
 * information to f if f is not NULL.  On success return the index of
 * the PDB loaded, on error set errno and return -1.
 */
static int
add_pdb(struct pdb_catalogue *cat, const char *tsbuf, const char *pdbdir, FILE *f)
{
	FILE *pdbfile;
	size_t pdbidx, len;
	tileset ts;
	int error;
	char pathbuf[PATH_MAX];

	if (tileset_parse(&ts, tsbuf) != 0) {
		if (f != NULL)
			fprintf(f, "Cannot parse tileset: %s\n", tsbuf);

		error = EINVAL;
		return (-1);
	}

	/* check if the PDB is already present */
	for (pdbidx = 0; pdbidx < cat->n_pdbs; pdbidx++)
		if (cat->pdbs_ts[pdbidx] == ts)
			return (pdbidx);


	/* if the PDB is not already present, allocate it */
	pdbidx = cat->n_pdbs++;
	if (pdbidx >= CATALOGUE_PDBS_LEN) {
		if (f != NULL)
			fprintf(f, "Too many PDBs, up to %d are possible.\n",
			    CATALOGUE_PDBS_LEN);

		error = ERANGE;
		return (-1);
	}

	cat->pdbs_ts[pdbidx] = ts;
	pdbfile = NULL;

	/* build the PDB's file name */
	if (pdbdir != NULL) {
		len = snprintf(pathbuf, sizeof pathbuf, "%s/%s.pdb", pdbdir, tsbuf);
		if (len >= sizeof pathbuf) {
			error = ENAMETOOLONG;
			return (-1);
		}

		if (f != NULL)
			fprintf(f, "Loading PDB file %s\n", pathbuf);

		pdbfile = fopen(pathbuf, "rb");
		if (pdbfile == NULL && errno != ENOENT) {
			error = errno;
			if (f != NULL)
				fprintf(f, "%s: %s\n", pathbuf, strerror(errno));
			errno = error;
			return (-1);
		}
	}


	/* if the PDB could not be found, generate it */
	if (pdbfile == NULL) {
		cat->pdbs[pdbidx] = pdb_allocate(ts);
		if (cat->pdbs[pdbidx] == NULL)
			return (-1);

		if (f != NULL)
			fprintf(f, "Generating PDB for tileset %s\n", tsbuf);

		pdb_generate(cat->pdbs[pdbidx], f);

		/* write PDB to disk if requested */
		if (pdbdir == NULL)
			return (pdbidx);

		if (f != NULL)
			fprintf(f, "Storing PDB to %s\n", pathbuf);

		pdbfile = fopen(pathbuf, "w+b");
		if (pdbfile == NULL || pdb_store(pdbfile, cat->pdbs[pdbidx]) != 0) {
			if (f != NULL)
				fprintf(f, "%s: %s\nContinuing anyway\n", pathbuf, strerror(errno));

			if (pdbfile != NULL)
				fclose(pdbfile);

			return (pdbidx);
		}

		rewind(pdbfile);
		pdb_free(cat->pdbs[pdbidx]);
	}

	/* map PDB into RAM */
	cat->pdbs[pdbidx] = pdb_mmap(ts, fileno(pdbfile), PDB_MAP_RDONLY);
	if (cat->pdbs[pdbidx] == NULL)
		return (-1);

	fclose(pdbfile);

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
catalogue_load(const char *catfile, const char *pdbdir, FILE *f)
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

		pdbidx = add_pdb(cat, linebuf, pdbdir, f);
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
		    cat->n_pdbs, cat->n_heuristics, catfile);

	fclose(catcfg);

	return (cat);

fail:
	fclose(catcfg);
	for (i = 0; i < cat->n_pdbs; i++)
		pdb_free(cat->pdbs[i]);

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

	for (i = 0; i < cat->n_pdbs; i++)
		pdb_free(cat->pdbs[i]);

	free(cat);
}

/*
 * Fill in a struct partial_hvals with values for puzzle configuration p
 * relative to PDB catalogue cat.  Return the best h value found in all
 * heuristics defined in cat.
 */
extern unsigned
catalogue_partial_hvals(struct partial_hvals *ph,
    struct pdb_catalogue *cat, const struct puzzle *p)
{
	size_t i;

	for (i = 0; i < cat->n_pdbs; i++)
		ph->hvals[i] = pdb_lookup_puzzle(cat->pdbs[i], p);

	return (catalogue_ph_hval(cat, ph));
}

/*
 * Update ph, a struct partial_hvals for a configuration neighboring p
 * by moving tile t, to contain partial h values for p.  To save time,
 * we only look up those PDB entries that changed when moving tile.
 */
extern unsigned
catalogue_diff_hvals(struct partial_hvals *ph, struct pdb_catalogue *cat,
    const struct puzzle *p, unsigned tile)
{
	size_t i;

	for (i = 0; i < cat->n_pdbs; i++)
		if (tileset_has(cat->pdbs_ts[i], tile))
			ph->hvals[i] = pdb_lookup_puzzle(cat->pdbs[i], p);

	return (catalogue_ph_hval(cat, ph));
}
