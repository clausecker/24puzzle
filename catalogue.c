/* catalogue.c -- pattern database catalogues */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "catalogue.h"
#include "pdb.h"
#include "puzzle.h"
#include "tileset.h"

enum { LINEBUF_LEN = 512 };

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
	struct patterndb *pdb;
	FILE *catcfg, *pdbfile = NULL;
	size_t i, had_empty_line = 1, len;
	int error, result, pdb_already_generated;
	tileset ts;
	char linebuf[LINEBUF_LEN], pathbuf[PATH_MAX], *newline;

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
		if (linebuf[0] == '\n') {
			if (!had_empty_line)
				cat->n_heuristics++;

			had_empty_line = 1;
			continue;
		}

		had_empty_line = 0;

		if (cat->n_heuristics >= HEURISTICS_LEN) {
			if (f != NULL)
				fprintf(f, "Too many heuristics, up to %d are possible.\n",
				    HEURISTICS_LEN);

			error = ERANGE;
			goto fail;
		}

		if (tileset_parse(&ts, linebuf) != 0) {
			if (f != NULL)
				fprintf(f, "Cannot parse tileset: %s\n", linebuf);

			error = EINVAL;
			goto fail;
		}

		/* check if the PDB is already present */
		for (i = 0; i < cat->n_pdbs; i++)
			if (cat->pdbs_ts[i] == ts) {
				cat->heuristics[i] |= 1 << cat->n_heuristics;
				cat->parts[cat->n_heuristics] |= 1 << i;
				goto continue_outer;
			}

		/* if the PDB is not already present, allocate it */
		if (cat->n_pdbs >= CATALOGUE_PDBS_LEN) {
			if (f != NULL)
				fprintf(f, "Too many PDBs, up to %d are possible.\n",
				    CATALOGUE_PDBS_LEN);

			error = ERANGE;
			goto fail;
		}

		pdbfile = NULL;
		pdb_already_generated = 0;

		/* check if the PDB is already present */
		if (pdbdir != NULL) {
			len = snprintf(pathbuf, sizeof pathbuf, "%s/%s.pdb", pdbdir, linebuf);
			if (len >= sizeof pathbuf) {
				error = ENAMETOOLONG;
				goto fail;
			}

			if (f != NULL)
				fprintf(f, "Loading PDB file %s\n", pathbuf);

			assert(pdbfile == NULL);
			pdbfile = fopen(pathbuf, "rb");
			if (pdbfile != NULL)
				pdb_already_generated = 1;
			else {
				if (f != NULL)
					fprintf(f, "%s: %s\n", pathbuf, strerror(errno));

				pdbfile = fopen(pathbuf, "w+b");
				if (pdbfile == NULL) {
					error = errno;
					if (f != NULL)
						fprintf(f, "%s: %s\n", pathbuf, strerror(error));

					goto fail;
				}
			}
		}

		/* if the PDB could not be found, generate it */
		pdb = NULL;
		if (!pdb_already_generated) {
			pdb = pdb_allocate(ts);
			if (pdb == NULL) {
				error = errno;
				goto fail;
			}

			if (f != NULL)
				fprintf(f, "Generating PDB for tileset %s\n", linebuf);

			pdb_generate(pdb, f);

			if (pdbfile != NULL) {
				if (f != NULL)
					fprintf(f, "Storing PDB to %s\n", pathbuf);

				result = pdb_store(pdbfile, pdb);
				error = errno;
				pdb_free(pdb);
				rewind(pdbfile);

				if (result != 0) {
					if (f != NULL)
						fprintf(f, "%s: %s\n", pathbuf, strerror(error));

					goto fail;
				}
			}
		}

		cat->pdbs_ts[cat->n_pdbs] = ts;
		if (pdbfile != NULL) {
			cat->pdbs[cat->n_pdbs] = pdb_mmap(ts, fileno(pdbfile), PDB_MAP_RDONLY);
			if (cat->pdbs[cat->n_pdbs] == NULL) {
				error = errno;
				goto fail;
			}

			fclose(pdbfile);
			pdbfile = NULL;
		} else {
			assert(pdb != NULL);
			cat->pdbs[cat->n_pdbs] = pdb;
		}

		cat->n_pdbs++;

	continue_outer:
		;
	}

	if (ferror(catcfg)) {
		error = errno;
		if (f != NULL)
			fprintf(f, "%s: %s\n", catfile, strerror(errno));

		goto fail;
	}

	if (!had_empty_line)
		cat->n_heuristics++;

	fclose(catcfg);

	return (cat);

fail:
	if (pdbfile != NULL)
		fclose(pdbfile);

	fclose(catcfg);
	for (i = 0; i < cat->n_pdbs; i++)
		pdb_free(cat->pdbs[i]);

earlyfail:
	free(cat);

	errno = error;
	return (NULL);
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
	unsigned long long parts;
	unsigned max = 0, sum;

	ph->fake_entries = 0;
	ph->maximums = 0;

	for (i = 0; i < cat->n_pdbs; i++)
		ph->hvals[i] = pdb_lookup_puzzle(cat->pdbs[i], p);

	for (i = 0; i < cat->n_heuristics; i++) {
		sum = 0;
		for (parts = cat->parts[i]; parts != 0; parts &= parts - 1)
			sum += ph->hvals[ctzll(parts)];

		if (sum > max) {
			max = sum;
			ph->maximums = 0;
		}

		if (sum == max)
			ph->maximums |= cat->parts[i];
	}

	return (max);
}
