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

/* bitpdbzstd.c -- functionality to deal with compressed bitpdbs */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <zstd.h>

#include "bitpdb.h"
#include "index.h"
#include "puzzle.h"
#include "tileset.h"

/*
 * Load a compressed bitpdb from pdbfile.  pdbfile must refer to an
 * ordinary file, the entirety of which contains the compressed bitpdb.
 */
extern struct bitpdb *
bitpdb_load_compressed(tileset ts, FILE *pdbfile)
{
	struct bitpdb *bpdb;
	struct stat st;
	size_t size, cap;
	void *inbuf;
	int error;

	bpdb = bitpdb_allocate(ts);
	if (bpdb == NULL)
		return (NULL);

	if (fstat(fileno(pdbfile), &st) != 0) {
		error = errno;
		goto fail1;
	}

	/* make sure we don't get wrong results due to overflow */
	assert(st.st_size >= 0);
	if (st.st_size != (off_t)(size_t)st.st_size) {
		error = EFBIG;
		goto fail1;
	}

	inbuf = malloc((size_t)st.st_size);
	if (inbuf == NULL) {
		error = errno;
		goto fail1;
	}

	cap = bitpdb_size(&bpdb->aux);
	rewind(pdbfile);
	size = fread(inbuf, 1, (size_t)st.st_size, pdbfile);
	if (size != (size_t)st.st_size) {
		error = errno;
		if (!ferror(pdbfile))
			error = EINVAL;
		goto fail2;
	}

	/* sanity check: make sure the PDB size matches */
	size = ZSTD_getFrameContentSize(inbuf, (size_t)st.st_size);
	if (size != cap) {
		error = EINVAL;
		goto fail2;
	}

	size = ZSTD_decompress(bpdb->data, cap, inbuf, (size_t)st.st_size);
	if (ZSTD_isError(size) || size != cap) {
		error = EINVAL;
		goto fail2;
	}

	free(inbuf);
	return (bpdb);

fail2:	free(inbuf);
fail1:	bitpdb_free(bpdb);
	errno = error;

	return (NULL);
}

/*
 * Compress bpdb and store the compressed data to pdbfile.
 * Return 0 on success, -1 on failure.
 */
extern int
bitpdb_store_compressed(FILE *pdbfile, struct bitpdb *bpdb)
{
	void *outbuf;
	size_t insize, outcap, outsize, size;
	int error;

	insize = bitpdb_size(&bpdb->aux);
	outcap = ZSTD_compressBound(insize);
	outbuf = malloc(outcap);
	if (outbuf == NULL)
		return (-1);

	outsize = ZSTD_compress(outbuf, outcap, bpdb->data, insize,
	    BITPDB_COMPRESSION_LEVEL);
	if (ZSTD_isError(outsize)) {
		error = EINVAL;
		goto fail;
	}

	size = fwrite(outbuf, 1, outsize, pdbfile);
	if (size != outsize) {
		error = errno;
		if (!ferror(pdbfile))
			error = EINVAL;

		goto fail;
	}

	free(outbuf);
	return (0);

fail:	free(outbuf);
	errno = error;

	return (-1);
}
