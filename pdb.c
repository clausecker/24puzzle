/* pdb.c -- PDB utility functions */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

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
	struct index_aux aux;
	struct patterndb *pdb;

	make_index_aux(&aux, ts);
	pdb = malloc(sizeof *pdb);
	if (pdb == NULL)
		return (NULL);

	pdb->aux = aux;
	pdb->mapped = 0;

	pdb->data = malloc(search_space_size(&pdb->aux));
	if (pdb->data == NULL) {
		free(pdb);
		return (NULL);
	}

	return (pdb);
}

/*
 * Release the storage allocated for pdb.
 */
extern void
pdb_free(struct patterndb *pdb)
{

	if (pdb->mapped)
		munmap(pdb->data, search_space_size(&pdb->aux));
	else
		free(pdb->data);

	free(pdb);
}

/*
 * Set each entry in PDB to UNREACHED.
 */
extern void
pdb_clear(struct patterndb *pdb)
{

	memset((void*)pdb->data, UNREACHED, search_space_size(&pdb->aux));
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
	size_t count, size = search_space_size(&pdb->aux);

	if (pdb == NULL)
		return (NULL);

	count = fread((void *)pdb->data, 1, size, pdbfile);
	if (count != size) {
		/* tell apart short read from IO error */
		if (!ferror(pdbfile))
			errno = EINVAL;

		pdb_free(pdb);
		return (NULL);
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
	size_t count, size = search_space_size(&pdb->aux);

	count = fwrite((void *)pdb->data, 1, size, pdbfile);
	if (count != size) {
		/* tell apart end of medium from IO error */
		if (!ferror(pdbfile))
			errno = ENOSPC;

		return (-1);
	}

	return (0);
}

/*
 * Load a PDB from file descriptor fd by mapping it into RAM.  This
 * might perform better than pdb_load().  Use flags to decide what
 * protection the mapping has and whether changes are written back to
 * the input file.
 */
extern struct patterndb *
pdb_mmap(tileset ts, int pdbfd, int mapflags)
{
	struct index_aux aux;
	struct patterndb *pdb;
	int prot, flags;

	switch (mapflags) {
	case PDB_MAP_RDONLY:
		prot = PROT_READ;
		flags = MAP_SHARED;
		break;

	case PDB_MAP_RDWR:
		prot = PROT_READ | PROT_WRITE;
		flags = MAP_PRIVATE;
		break;

	case PDB_MAP_SHARED:
		prot = PROT_READ | PROT_WRITE;
		flags = MAP_SHARED;
		break;

	default:
		errno = EINVAL;
		return (NULL);
	}

	make_index_aux(&aux, ts);
	pdb = malloc(sizeof *pdb);
	if (pdb == NULL)
		return (NULL);

	pdb->aux = aux;
	pdb->mapped = 1;
	pdb->data = mmap(NULL, search_space_size(&pdb->aux), prot, flags, pdbfd, 0);
	if (pdb->data == MAP_FAILED) {
		free(pdb);
		return (NULL);
	}

	return (pdb);
}
