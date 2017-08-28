/* pdb.c -- PDB utility functions */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tileset.h"
#include "index.h"
#include "puzzle.h"
#include "pdb.h"

/*
 * Allocate storage for a pattern database representing ts.  If storage
 * is insufficient, return NULL and set errno.  The entries in PDB are
 * undefined initially.  Use pdb_clear() to set the patterndb to a
 * well-defined state.
 */
extern struct patterndb *
pdb_allocate(tileset ts)
{
	size_t i, n_tables = maprank_count(ts);
	struct patterndb *pdb = malloc(sizeof *pdb + sizeof *pdb->tables * n_tables);

	if (pdb == NULL)
		return (NULL);

	make_index_aux(&pdb->aux, ts);

	/* allow us to simply call pdb_free() if we run out of memory */
	memset(pdb->tables, 0, sizeof *pdb->tables * n_tables);

	for (i = 0; i < n_tables; i++) {
		pdb->tables[i] = malloc(pdb_table_size(pdb, i));
		if (pdb->tables[i] == NULL) {
			pdb_free(pdb);
			return (NULL);
		}
	}

	return (pdb);
}

/*
 * Release the storage allocated for pdb.
 */
extern void
pdb_free(struct patterndb *pdb)
{
	size_t i, n_tables = maprank_count(pdb->aux.ts);

	for (i = 0; i < n_tables; i++)
		free(pdb->tables[i]);

	free(pdb);
}

/*
 * Set each entry in PDB to UNREACHED.
 */
extern void
pdb_clear(struct patterndb *pdb)
{
	size_t i, n_tables = maprank_count(pdb->aux.ts);

	for (i = 0; i < n_tables; i++) {
		memset((void *)pdb->tables[i], UNREACHED, pdb_table_size(pdb, i));
	}
}

/*
 * Load a PDB for tileset ts from file and return a pointer to the PDB
 * just loaded.  On error, return NULL and set errno to indicate the
 * problem.  pdbfile must be a binary file opened for reading with the
 * file pointer positioned right at the beginning of the PDB.  The file
 * pointer is located at the end of the PDB on success and is undefined
 * on failure.
 */
extern struct patterndb *
pdb_load(tileset ts, FILE *pdbfile)
{
	struct patterndb *pdb = pdb_allocate(ts);
	size_t i, n_tables = maprank_count(ts), count, tblsize;

	if (pdb == NULL)
		return (NULL);

	for (i = 0; i < n_tables; i++) {
		tblsize = pdb_table_size(pdb, i);
		count = fread((void *)pdb->tables[i], 1, tblsize, pdbfile);
		if (count != tblsize) {
			/* tell apart short read from IO error */
			if (!ferror(pdbfile))
				errno = EINVAL;

			pdb_free(pdb);
			return (NULL);
		}
	}

	return (pdb);
}

/*
 * Write pdb to pdbfile.  Return 0 an success.  On error, return -1 and
 * set errno to indicate the cause of the error.  pdbfile must be a
 * binary file open for writing.  The file pointer is positioned after
 * the end of the PDB on success, undefined on failure.
 */
extern int
pdb_store(FILE *pdbfile, struct patterndb *pdb)
{
	size_t i, n_tables = maprank_count(pdb->aux.ts), count, tblsize;

	for (i = 0; i < n_tables; i++) {
		tblsize = pdb_table_size(pdb, i);
		count = fwrite((void *)pdb->tables[i], 1, tblsize, pdbfile);
		if (count != tblsize) {
			/* tell apart end of medium from IO error */
			if (!ferror(pdbfile))
				errno = ENOSPC;

			return (-1);
		}
	}

	return (0);
}
