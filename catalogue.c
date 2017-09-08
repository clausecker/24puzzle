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
 * Load a catalogue from catfile, search for PDBs in pdbdir.  Print
 * status information to f if f is not NULL.  A catalogue file contains
 * groups of tilesets.  Each group forms one heuristic.  The tileset
 * name is used to form a file name for the PDB, so the order of
 * components should be the same every time.  If the same PDB is used in
 * multiple heuristics, it is loaded only once still.  For better
 * performance, the PDB is loaded as a memory mapped file.  On error,
 * NULL is returned and errno set to indicate the problem.
 */
extern struct pdb_catalogue *
catalogue_load(const char *catfile, const char *pdbdir, FILE *f)
{
	struct pdb_catalogue *cat = malloc(sizeof *cat);
	size_t i, had_empty_line = 1, len;
	tileset ts;
	int fd;
	char linebuf[LINEBUF_LEN], pathbuf[PATH_MAX];
	FILE *catcfg;

	if (cat == NULL)
		return (NULL);

	memset(cat, 0, sizeof *cat);

	if (f != NULL)
		fprintf(f, "Loading PDB catalogue from %s\n", catfile);

	catcfg = fopen(catfile, "r");
	if (catcfg == NULL)
		goto earlyfail;

	while (fgets(linebuf, sizeof linebuf, catcfg) != NULL) {
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

			errno = ERANGE;
			goto fail;
		}

		if (tileset_parse(&ts, linebuf) != 0) {
				if (f != NULL)
					fprintf(f, "Cannot parse tileset: %s\n", linebuf);

				errno = EINVAL;
				goto fail;
		}

		/* check if the PDB is already present, if not, allocate it */
		for (i = 0; i < cat->n_pdbs; i++)
			if (cat->pdbs_ts[i] == ts) {
				cat->heuristics[i] |= 1 << cat->n_heuristics;
				cat->parts[cat->n_heuristics] |= 1 << i;
				continue;
			}

		/* if the PDB is not already present, allocate it */
		if (cat->n_pdbs >= CATALOGUE_PDBS_LEN) {
			if (f != NULL)
				fprintf(f, "Too many PDBs, up to %d are possible.\n",
				    CATALOGUE_PDBS_LEN);

			errno = ERANGE;
			goto fail;
		}

		linebuf[strcspn(linebuf, " \n")] = '\0';
		len = snprintf(pathbuf, sizeof pathbuf, "%s/%s.pdb", pdbdir, linebuf);
		if (len >= sizeof pathbuf) {
			errno = ENAMETOOLONG;
			goto fail;
		}

		if (f != NULL)
			fprintf(f, "Loading PDB file %s\n", pathbuf);

		fd = open(pathbuf, O_RDONLY);
		if (fd == -1)
			goto fail;

		cat->pdbs_ts[cat->n_pdbs] = ts;
		cat->pdbs[cat->n_pdbs] = pdb_mmap(ts, fd, PDB_MAP_RDONLY);
		if (cat->pdbs[cat->n_pdbs] == NULL)
			goto fail;

		close(fd);
		cat->n_pdbs++;
	}

	if (ferror(catcfg))
		goto fail;

	if (!had_empty_line)
		cat->n_heuristics++;

	fclose(catcfg);

	return (cat);

fail:
	fclose(catcfg);
	for (i = 0; i < cat->n_pdbs; i++)
		pdb_free(cat->pdbs[i]);

earlyfail:
	free(cat);

	return (NULL);
}
