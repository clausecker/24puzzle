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

#ifndef PUZZLE_H
#define PUZZLE_H

#include <assert.h>
#include <stdalign.h>
#include <stdatomic.h>
#include <stddef.h>

/*
 * One configuration of a 24 puzzle.  A 24 puzzle configuration
 * comprises 24 tiles labelled 1 to 24 arranged in a 5x5 grid with one
 * spot remaining empty.  The goal of the puzzle is to arrange the
 * tiles like on the left:
 *
 *     []  1  2  3  4       1  2  3  4  5
 *      5  6  7  8  9       6  7  8  9 10
 *     10 11 12 13 14      11 12 13 14 15
 *     15 16 17 18 19      16 17 18 19 20
 *     20 21 22 23 24      21 22 23 24 []
 *
 * Note that this arrangement is different from the traditional
 * arrangement on the right.  It is however isomorphic to the
 * traditional tile arrangement by changing coordinates and tile
 * numbers.
 *
 * To simplify the algorithms we want to run on them, puzzle
 * configurations are stored in two ways: First, the position of
 * each tile is stored in tiles[], then, the tile on each grid
 * position is stored in grid[] with 0 indicating the empty spot.
 * If viewed as permutations of { 0, ..., 24 }, tiles[] and grid[]
 * are inverse to each other at any given time.
 */
enum { TILE_COUNT = 25, ZERO_TILE = 0 };

/*
 * The branching factor of the 24 puzzle's search space.  This is
 * just sqrt(2 + sqrt(13)).
 */
#define B 2.367604543724308131130874

struct puzzle {
	alignas(8) unsigned char tiles[TILE_COUNT], grid[TILE_COUNT];
};

/* puzzle.c */
extern const struct puzzle solved_puzzle;
extern const signed char movetab[TILE_COUNT][4];
extern const unsigned char moveidx_diffs[2 * TILE_COUNT - 1];
extern const signed char moveidx_idxs[TILE_COUNT][5];

enum { PUZZLE_STR_LEN = 3 * TILE_COUNT + 1 };

extern int	puzzle_parity(const struct puzzle *);
extern void	puzzle_string(char[PUZZLE_STR_LEN], const struct puzzle *);
extern void	puzzle_visualization(char[PUZZLE_STR_LEN], const struct puzzle *);
extern int	puzzle_parse(struct puzzle *, const char *);
/*
 * Return the location of the zero tile in p.
 */
static inline size_t
zero_location(const struct puzzle *p)
{
	return (p->tiles[ZERO_TILE]);
}

/*
 * Move the empty square to dloc, modifying p.  It is not tested whether
 * dest is adjacent to the empty square's current location.  Furtermore,
 * this function assumes 0 <= dloc < 25.
 */
static inline void
move(struct puzzle *p, size_t dloc)
{
	size_t zloc, dtile;

	dtile = p->grid[dloc];
	zloc = zero_location(p);

	p->grid[dloc] = ZERO_TILE;
	p->grid[zloc] = dtile;

	p->tiles[dtile] = zloc;
	p->tiles[ZERO_TILE] = dloc;
}

/*
 * Return the number of moves when the empty square is at z.  It is
 * assumed that 0 <= z < 25.
 */
static inline size_t
move_count(size_t z)
{
	/*
	 * 0xefffee is 01110 11111 11111 11111 01110,
	 * 0x07e9c0 is 00000 01110 01110 01110 00000,
	 * i.e. everything but the corners and everything but the border.
	 */
	return (2 + ((0xefffee & 1 << z) != 0) + ((0x0739c0 & 1 << z) != 0));
}

/*
 * Return the possible moves from square z.  Up to four moves are
 * possible, the exact number can be found using move_count(z).  If
 * less than four moves are possible, the last one or two entries are
 * marked with -1.  It is assumed that 0 <= z < 25.
 */
static inline const signed char *
get_moves(size_t z)
{
	return (movetab[z]);
}

/*
 * Compute an index such that get_moves(a)[move_index(a, b)] == b.
 */
static inline int
move_index(int a, int b)
{
	int idx = moveidx_idxs[a][moveidx_diffs[b - a + TILE_COUNT - 1]];

	assert(idx != -1);

	return (idx);
}

/* validation.c */
extern int	puzzle_valid(const struct puzzle *);

#endif /* PUZZLE_H */
