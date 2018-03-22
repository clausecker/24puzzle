/*-
 * Copyright (c) 2017--2018 Robert Clausecker. All rights reserved.
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

/* the number of PDBs with the given size, i.e. (TILE_COUNT choose i) */
const unsigned pdbcount[TILE_COUNT] = {
	1,
	24,
	276,
	2024,
	10626,
	42504,
	134596,
	346104,
	735471,
	1307504,
	1961256,
	2496144,
	2704156,
	2496144,
	1961256,
	1307504,
	735471,
	346104,
	134596,
	42504,
	10626,
	2024,
	276,
	24,
	1
};

/*
 * Allocate a struct patterndb for tileset ts and fill in all fields as
 * appropriate.  Do not allocate any backing storage.  Return NULL in
 * case of failure.  This function is mainly useful in case you want to
 * use pdb_iterate_parallel without actually having an underlying PDB.
 */
extern struct patterndb *
pdb_dummy(tileset ts)
{
	struct patterndb *pdb;

	pdb = malloc(sizeof *pdb);
	if (pdb == NULL)
		return (NULL);

	make_index_aux(&pdb->aux, ts);
	pdb->mapped = 0;
	pdb->data = NULL;

	return (pdb);
}

/*
 * Allocate storage for a pattern database representing ts.  If storage
 * is insufficient, return NULL and set errno.  The entries in PDB are
 * undefined initially.  Use pdb_clear() to set the patterndb to a
 * well-defined state.
 */
extern struct patterndb *
pdb_allocate(tileset ts)
{
	struct patterndb *pdb = pdb_dummy(ts);
	int error;

	if (pdb == NULL)
		return (NULL);

	pdb->data = malloc(search_space_size(&pdb->aux));
	if (pdb->data == NULL) {
		error = errno;
		free(pdb);
		errno = error;
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
	size_t count, size;
	int error;

	if (pdb == NULL)
		return (NULL);

	size = search_space_size(&pdb->aux);
	count = fread((void *)pdb->data, 1, size, pdbfile);
	if (count != size) {
		error = errno;
		pdb_free(pdb);

		/* tell apart short read from IO error */
		if (!ferror(pdbfile))
			errno = EINVAL;
		else
			errno = error;

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
	int error;

	count = fwrite((void *)pdb->data, 1, size, pdbfile);
	if (count != size) {
		error = errno;

		/* tell apart end of medium from IO error */
		if (!ferror(pdbfile))
			errno = ENOSPC;
		else
			errno = error;

		return (-1);
	}

	fflush(pdbfile);

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
	struct patterndb *pdb = pdb_dummy(ts);
	int prot, flags, error;

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

	if (pdb == NULL)
		return (NULL);

	pdb->mapped = 1;
	pdb->data = mmap(NULL, search_space_size(&pdb->aux), prot, flags, pdbfd, 0);
	if (pdb->data == MAP_FAILED) {
		error = errno;
		free(pdb);
		errno = error;
		return (NULL);
	}

	return (pdb);
}
