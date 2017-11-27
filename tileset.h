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

#ifndef TILESET_H
#define TILESET_H

#include "builtins.h"
#include "puzzle.h"

/*
 * For many purposes, it is useful to consider sets of tiles.
 * A tileset represents a set of tiles as a bitmask.
 */
typedef unsigned tileset;

enum {
	EMPTY_TILESET = 0,
	FULL_TILESET = (1 << TILE_COUNT) - 1,
	DEFAULT_TILESET = 0x00000e7, /* 0 1 2 5 6 7 */

	TILESET_STR_LEN = 3 * TILE_COUNT + 1,
	/* 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,\0 */
	TILESET_LIST_LEN = 66,
};

extern unsigned	tileset_populate_eqclasses(signed char[TILE_COUNT], tileset);
extern void	tileset_string(char[TILESET_STR_LEN], tileset);
extern void	tileset_list_string(char[TILESET_LIST_LEN], tileset);
extern int	tileset_parse(tileset *, const char *);

/*
 * Return 1 if t is in ts.
 */
static inline int
tileset_has(tileset ts, unsigned t)
{
	return ((ts & (tileset)1 << t) != 0);
}

/*
 * Add t to ts and return the updated tileset.
 */
static inline tileset
tileset_add(tileset ts, unsigned t)
{
	return (ts | (tileset)1 << t);
}

/*
 * Return a tileset containing all tiles in a that are not in b.
 */
static inline tileset
tileset_difference(tileset a, tileset b)
{
	return (a & ~b);
}

/*
 * Remove t from ts and return the updated tileset.
 */
static inline tileset
tileset_remove(tileset ts, unsigned t)
{
	return (tileset_difference(ts, tileset_add(EMPTY_TILESET, t)));
}

/*
 * Return the number of tiles in ts.
 */
static inline unsigned
tileset_count(tileset ts)
{
	return (popcount(ts));
}

/*
 * Return 1 if ts is empty.
 */
static inline int
tileset_empty(tileset ts)
{
	return (ts == 0);
}

/*
 * Return a tileset containing all tiles not in ts.
 */
static inline tileset
tileset_complement(tileset ts)
{
	return (~ts & FULL_TILESET);
}

/*
 * Return a tileset equal to ts without the tile with the lowest number.
 * If ts is empty, return ts unchanged.
 */
static inline tileset
tileset_remove_least(tileset ts)
{
	return (ts & ts - 1);
}

/*
 * Return the number of the lowest numbered tile in ts.  If ts is empty,
 * behaviour is undefined.
 */
static inline unsigned
tileset_get_least(tileset ts)
{
	return (ctz(ts));
}

/*
 * Return the intersection of ts1 and ts2.
 */
static inline tileset
tileset_intersect(tileset ts1, tileset ts2)
{
	return (ts1 & ts2);
}

/*
 * Return the union of ts1 and ts2.
 */
static inline tileset
tileset_union(tileset ts1, tileset ts2)
{
	return (ts1 | ts2);
}

/*
 * Return a tileset containing the lowest numbered n tiles.
 */
static inline tileset
tileset_least(unsigned n)
{
	return ((1 << n) - 1);
}

/*
 * Return the parity of a tileset.  This is whether the number of
 * even-numbered tiles is even or odd.
 */
static inline tileset
tileset_parity(tileset ts)
{
	return (tileset_count(ts & 0x1555555) & 1);
}

/*
 * Given a tileset eq representing some squares of the grid, return a
 * tileset composed of those squares in eq which are adjacent to a
 * square not in eq.  Intuitively, these are the squares from which a
 * move could possibly lead to a configuration in a different
 * equivalence class.
 */
static inline tileset
tileset_reduce_eqclass(tileset eq)
{
	tileset c = tileset_complement(eq);

	/*
	 * the mask prevents carry into other rows:
	 * 0x0f7bdef: 01111 01111 01111 01111 01111
	 */
	return (eq & (c | c  << 5 | (c & 0x0f7bdef) << 1 | c >> 5 | c >> 1 & 0x0f7bdef));
}

/*
 * Given a tileset cmap representing free positions on the grid and
 * a square number t set in cmap, return a tileset representing
 * all squares reachable from t through members of ts.
 */
static inline tileset
tileset_flood(tileset cmap, unsigned t)
{
        tileset r = tileset_add(EMPTY_TILESET, t), oldr;

        do {
                oldr = r;

                /*
                 * the mask prevents carry into other rows:
                 * 0x0f7bdef: 01111 01111 01111 01111 01111
                 */
                r = cmap & (r | r  << 5 | (r & 0x0f7bdef) << 1 | r >> 5 | r >> 1 & 0x0f7bdef);
        } while (oldr != r);

        return (r);
}


/*
 * Given a puzzle configuration p, a tileset ts and a tileset eq =
 * tileset_eqclass(ts, p), return if p is the canonical configuration in
 * p.  A configuration in the equivalence class is canonical, if it has
 * the lowest zero position out of all configurations in the equivalence
 * class that are equal with respect to ts.  Return nonzero if this is
 * the canonical configuration, zero if it is not.
 */
static inline int
tileset_is_canonical(tileset ts, tileset eq, const struct puzzle *p)
{

	if (tileset_has(ts, ZERO_TILE))
		return (zero_location(p) == tileset_get_least(eq));
	else
		return (1);
}

/*
 * The rank of a tileset.  Given a tileset ts, its rank is the position
 * in lexicographic order it has among all tilesets with the same tile
 * count.
 */
typedef unsigned tsrank;

enum { RANK_SPLIT1 = 11, RANK_SPLIT2 = 18 };

/* ranktbl.c generated by util/rankgen */
extern const unsigned short rank_tails[1 << RANK_SPLIT1];
extern const tsrank rank_mids[RANK_SPLIT1 + 1][1 << RANK_SPLIT2 - RANK_SPLIT1];
extern const tsrank rank_heads[RANK_SPLIT2 + 1][1 << TILE_COUNT - RANK_SPLIT2];

/* rank.c */
extern const tileset *unrank_tables[TILE_COUNT + 1];
extern const tsrank combination_count[TILE_COUNT + 1];

extern void	tileset_unrank_init(size_t);

/*
 * Compute the rank of a tile set.  tileset_rank_init() must have been
 * called before this function.
 */
static inline tsrank
tileset_rank(tileset ts)
{
	tileset tail = tileset_intersect(ts, tileset_least(RANK_SPLIT1));
	tileset mid  = tileset_intersect(ts, tileset_least(RANK_SPLIT2));
	tileset head = ts >> RANK_SPLIT2;

	return (rank_tails[tail] + rank_mids[tileset_count(tail)][mid >> RANK_SPLIT1]
	    + rank_heads[tileset_count(mid)][head]);
}

/*
 * Compute the tileset with k tiles belonging to rank rk.  The unrank
 * table for k must be initialized by a call to tileset_unrank_init(k)
 * beforehand.
 */
static inline tileset
tileset_unrank(size_t k, tsrank rk)
{
	return (unrank_tables[k][rk]);
}

/*
 * Compute the lexicographically next combination with tileset_count(ts)
 * bits to ts and return it.
 */
static inline tileset
next_combination(tileset ts)
{
	/* https://graphics.stanford.edu/~seander/bithacks.html */
	tileset t = ts | ts - 1;

	return (t + 1 | (~t & -~t) - 1 >> tileset_get_least(ts) + 1);
}

/* moves.c */

/*
 * A structure indicating a move with the zero tile being in zloc and
 * moving to dest.
 */
struct move {
	unsigned zloc, dest;
};

/* The maximum number of moves generate_moves() could generate. */
enum { MAX_MOVES = 4 * 9 + 3 * 12 + 2 * 4 }; /* TODO: a lower size is probably possible */

extern size_t	generate_moves(struct move[MAX_MOVES], tileset);

#endif /* TILESET_H */
