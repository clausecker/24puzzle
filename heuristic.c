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

/* heuristic.c -- abstraction over different heuristic types */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "heuristic.h"
#include "transposition.h"
#include "tileset.h"
#include "puzzle.h"
#include "pdb.h"

/*
 * A heuristic function driver.  Each driver is responsible for some
 * heuristic types as described in the drivers array.  drivers behave
 * similar to heu_open(), except HEU_NOMORPH is ignored and heu->ts
 * and heu->morphism must not be touched.  tsstr points to the output
 * of tileset_list_string() applied to ts.
 */
typedef int heu_driver(struct heuristic *heu, const char *heudir,
    tileset ts, char *tsstr, int flags);

static heu_driver pdb_driver, ipdb_driver, zpdb_driver;

/*
 * All available drivers.  The array is terminated with a NULL sentinel.
 * If the HEU_SIMILAR flag is provided in flags, this entry is only to
 * be used if HEU_SIMILAR was provided to heu_open().
 */
const struct {
	const char *typestr;
	heu_driver *drvfun;
	int flags;
} drivers[] = {
	"pdb",	pdb_driver, 0,
	"ipdb", ipdb_driver, 0,
	"zpdb", zpdb_driver, 0,
	NULL,	NULL, 0,
};

/*
 * In heudir, try to find a file describing a heuristic of type typestr
 * for tile set ts and open it.  If this is succesful, return 0 and
 * populate heu.  If this is unsuccesful, return -1 and set errno to
 * indicate an error.  If HEU_CREATE is set, create the heuristic if
 * not present.  If HEU_VERBOSE is set, print status information to
 * stderr.  If HEU_NOMORPH is set, do not look for isomorphic
 * heuristics.  If HEU_SIMILAR is set, look for different
 * representations of the same heuristic type, too.
 */
extern int
heu_open(struct heuristic *heu,
    const char *heudir, tileset ts, const char *typestr, int flags)
{
	size_t i;
	int saved_errno = errno, type_match = 0;
	char tsstr[TILESET_LIST_LEN];

	ts = tileset_remove(ts, ZERO_TILE);
	heu->ts = ts;

	if (flags & HEU_NOMORPH) {
		heu->morphism = canonical_automorphism(ts);
		ts = tileset_morph(ts, heu->morphism);
	} else
		heu->morphism = 0;

	tileset_list_string(tsstr, ts);

	/* is there an exact match? */
	for (i = 0; drivers[i].typestr != NULL; i++) {
		if (drivers[i].flags & HEU_SIMILAR || strcmp(typestr, drivers[i].typestr) != 0)
			continue;

		type_match = 1;

		if (drivers[i].drvfun(heu, heudir, ts, tsstr, flags & ~HEU_CREATE) == 0)
			goto success;
	}

	/* is there a similar match? */
	if (flags & HEU_SIMILAR)
		for (i = 0; drivers[i].typestr != NULL; i++) {
			if (!(drivers[i].flags & HEU_SIMILAR)
			    || strcmp(typestr, drivers[i].typestr) != 0)
				continue;

			type_match = 1;

			if (drivers[i].drvfun(heu, heudir, ts, tsstr, flags & ~HEU_CREATE) == 0)
				goto success;
		}

	/* can we create a heuristic? */
	if (flags & HEU_CREATE)
		for (i = 0; drivers[i].typestr != NULL; i++) {
			if (drivers[i].flags & HEU_SIMILAR || strcmp(typestr, drivers[i].typestr) != 0)
				continue;

			if (drivers[i].drvfun(heu, heudir, ts, tsstr, flags) == 0)
				goto success;

			/* drvfun failed to create the heuristic */
			saved_errno = errno;
			if (flags & HEU_VERBOSE)
				fprintf(stderr, "Could not create heuristic for tileset %s "
				    "of type %s: %s\n", tsstr, typestr, strerror(saved_errno));

			errno = saved_errno;
			return (-1);
		}

	if (type_match) {
		if (flags & HEU_VERBOSE)
			fprintf(stderr, "No heuristic for tileset %s of type %s%s found!\n",
			    tsstr, typestr, flags & HEU_SIMILAR ? " or similar" : "");

		errno = ENOENT;
		return (-1);
	} else {
		if (flags & HEU_VERBOSE)
			fprintf(stderr, "Unrecognized heuristic type %s for tile set %s\n",
			    typestr, tsstr);

		errno = EINVAL;
		return (-1);
	}

success:
	errno = saved_errno;
	return (0);
}

/*
 * Look up the h value provided by heu for p, using old_h for old_p as a
 * reference.  p_old must be adjacent to p.
 */
extern unsigned
heu_diff_hval(struct heuristic *heu, const struct puzzle *p,
    const struct puzzle *old_p, int old_h)
{
	struct puzzle p_morphed, old_p_morphed;
	const struct puzzle *pp = p, *old_pp = old_p;

	if (heu->morphism != 0) {
		p_morphed = *p;
		morph(&p_morphed, heu->morphism);
		pp = &p_morphed;

		old_p_morphed = *old_p;
		morph(&old_p_morphed, heu->morphism);
		old_pp = &old_p_morphed;
	}

	return (heu->hdiff(heu->provider, pp, old_pp, old_h));
}


/*
 * hval, hdiff, and free implementations for struct patterndb based heuristics.
 */
static int
pdb_hval_wrapper(void *provider, const struct puzzle *p)
{

	return (pdb_lookup_puzzle((struct patterndb *)provider, p));
}

static int
pdb_hdiff_wrapper(void *provider, const struct puzzle *p,
    const struct puzzle *old_p, int old_h)
{

	(void)old_p;
	(void)old_h;

	return (pdb_lookup_puzzle((struct patterndb *)provider, p));
}

static void
pdb_free_wrapper(void *provider)
{

	pdb_free((struct patterndb *)provider);
}

/*
 * The common code to drive struct patterndb base pattern databases.
 * suffix is the file suffix we use to find the pattern database,
 * identify is 1 if we are looking for an identified PDB.
 */
static int
common_pdb_driver(struct heuristic *heu, const char *heudir,
    tileset ts, char *tsstr, int flags, const char *suffix, int identify)
{
	FILE *pdbfile;
	struct patterndb *pdb;
	int fd, saved_errno;
	char pathbuf[PATH_MAX];

	if (heudir == NULL) {
		if (flags & HEU_CREATE)
			goto create_pdb;

		errno = EINVAL;
		return (-1);
	}

	if (snprintf(pathbuf, PATH_MAX, "%s/%s.%s", heudir, tsstr, suffix) >= PATH_MAX) {
		errno = ENAMETOOLONG;
		if (flags & HEU_VERBOSE) {
			perror("common_pdb_driver");
			errno = ENAMETOOLONG;
		}

		return (-1);
	}

	fd = open(pathbuf, O_RDONLY);
	if (fd == -1) {
		/* don't annoy the user with useless ENOENT messages */
		if (flags & HEU_VERBOSE && errno != ENOENT) {
			saved_errno = errno;
			perror(pathbuf);
			errno = saved_errno;
		}

		if (flags & HEU_CREATE)
			goto create_pdb;
		else
			return (-1);
	}

	if (flags & HEU_VERBOSE)
		fprintf(stderr, "Loading PDB file %s\n", pathbuf);

	pdb = pdb_mmap(ts, fd, PDB_MAP_RDONLY);
	saved_errno = errno;
	close(fd);

	/*
	 * if we can open the file but not map the PDB,
	 * something went terribly wrong and we don't want to ignore
	 * that error.
	 */
	if (pdb == NULL) {
		errno = saved_errno;
		if (flags & HEU_VERBOSE) {
			perror("pdb_mmap");
			errno = saved_errno;
		}

		return (-1);
	}

	goto success;

create_pdb:
	if (flags & HEU_VERBOSE)
		fprintf(stderr, "Creating PDB for tile set %s\n", tsstr);

	pdb = pdb_allocate(identify ? tileset_add(ts, ZERO_TILE) : ts);
	if (pdb == NULL) {
		if (flags & HEU_VERBOSE) {
			saved_errno = errno;
			perror("pdb_allocate");
			errno = saved_errno;
		}

		return (-1);
	}

	if (heudir == NULL)
		pdbfile = NULL;
	else {

		pdbfile = fopen(pathbuf, "wb");

		/*
		 * if the file can't be opened for writing, proceed
		 * with the generation but don't write the PDB back
		 * to disk.
		 */
		if (pdbfile == NULL && flags & HEU_VERBOSE)
			perror(pathbuf);
	}

	pdb_generate(pdb, flags & HEU_VERBOSE ? stderr : NULL);

	if (identify) {
		if (flags & HEU_VERBOSE)
			fprintf(stderr, "Identifying PDB for tile set %s\n", tsstr);

		pdb_identify(pdb);
	}

	if (pdbfile == NULL)
		goto success;

	if (flags & HEU_VERBOSE)
		fprintf(stderr, "Writing PDB to file %s\n", pathbuf);

	if (pdb_store(pdbfile, pdb) != 0) {
		if (flags & HEU_VERBOSE)
			perror("pdb_store");

		fclose(pdbfile);
		goto success;
	}

	pdb_free(pdb);
	pdb = pdb_mmap(ts, fileno(pdbfile), PDB_MAP_RDONLY);
	saved_errno = errno;
	fclose(pdbfile);

	if (pdb == NULL) {
		if (flags & HEU_VERBOSE) {
			errno = saved_errno;
			perror("pdb_mmap");
		}

		errno = saved_errno;
		return (-1);
	}

success:
	heu->provider = pdb;
	heu->hval = pdb_hval_wrapper;
	heu->hdiff = pdb_hdiff_wrapper;
	heu->free = pdb_free_wrapper;

	return (0);

}

/*
 * Driver for PDBs that do not account for the zero tile.
 */
static int
pdb_driver(struct heuristic *heu, const char *heudir,
    tileset ts, char *tsstr, int flags)
{
	return (common_pdb_driver(heu, heudir, ts, tsstr, flags, "pdb", 0));
}

/*
 * Driver for identified PDBs.
 */
static int
ipdb_driver(struct heuristic *heu, const char *heudir,
    tileset ts, char *tsstr, int flags)
{
	return (common_pdb_driver(heu, heudir, ts, tsstr, flags, "ipdb", 1));
}

/*
 * Driver for PDBs that do account for the zero tile.
 */
static int
zpdb_driver(struct heuristic *heu, const char *heudir,
    tileset ts, char *tsstr, int flags)
{

	ts = tileset_add(ts, ZERO_TILE);
	tileset_list_string(tsstr, ts);

	return (common_pdb_driver(heu, heudir, ts, tsstr, flags, "pdb", 0));
}
