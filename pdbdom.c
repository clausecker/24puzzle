/* pdbdom.c -- approximate minimal dominating sets to reduce PDBs */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "tileset.h"
#include "index.h"
#include "puzzle.h"
#include "pdb.h"

/*
 * One entry of an equidistance array. p is the index of the puzzle
 * configuration corresponding to this entry, additions is either the
 * number of as-of-now uncovered vertex in the child generation adding
 * this vertex to the dominating set would dominate or DOMINATED if this
 * configuration is either part of the dominating set or dominated by
 * some other vertex.
 */
struct vertex {
	unsigned additions : 8; /* number of new configurations this would cover */
	cmbindex index : 56;    /* puzzle configuration */
};

enum {
	DOMINATED = 0xff,
	REACH_LEN = 256,	/* TODO, see compute_reach() */
};

/*
 * The binary heap represented by a pointer to its backing array.
 * length contains the length of the array that is inserted into the
 * tree, total contains the number of elements in the backing array in
 * total.
 */
struct heap {
	struct vertex *root;
	size_t length, total;
};

/*
 * Returns the index of the left child of the vertex with index n.
 */
static inline size_t
left_child(size_t n)
{
	return (2 * n + 1);
}

/*
 * Returns the index of the right child of the vertex with index n.
 */
static inline size_t
right_child(size_t n)
{
	return (2 * n + 2);
}

#if 0
/*
 * Return the index of the parent of the vertex with index n.
 */
static inline size_t
parent(size_t n)
{
	return (n - 1 >> 1);
}
#endif

/*
 * Compare vertices i and j in the binary heap and return nonzero if
 * vertex i is less than vertex j.
 */
static inline int
less_than(struct heap *heap, size_t i, size_t j)
{
	return (heap->root[i].additions < heap->root[j].additions);
}

/*
 * Exchange vertices i and j in the binary heap.
 */
static inline void
exchange(struct heap *heap, size_t i, size_t j)
{
	struct vertex ival, jval;

	ival = heap->root[i];
	jval = heap->root[j];

	heap->root[i] = jval;
	heap->root[j] = ival;
}

#if 0
/*
 * Insert the element at entry length into the heap by moving it up the
 * heap until the heap property is satisfied.  Afterwards, increment
 * heap->length.
 */
static void
insert(struct heap *heap)
{
	size_t i = heap->length;

	while (i > 0) {
		if (!less_than(heap, parent(i), i))
			break;

		exchange(heap, parent(i), i);
		i = parent(i);
	}
}
#endif

/*
 * Return a pointer to the element at the root of the binary heap.
 */
static inline struct vertex *
get_root(struct heap *heap)
{
	return (heap->root);
}

/*
 * Restore the heap property for vertex i.  Return nonzero if the root
 * vertex had to be sifted down the heap, zero if the tree is unchanged.
 */
static int
heapify(struct heap *heap, size_t i)
{
	size_t parent = i, max, left, right;

	for (;;) {
		max = parent;
		left = left_child(parent);
		right = right_child(parent);

		if (left < heap->length && less_than(heap, max, left))
			max = left;

		if (right < heap->length && less_than(heap, max, right))
			max = right;

		if (max == parent)
			break;

		exchange(heap, max, parent);
		parent = max;
	}

	return (max == i);
}


/*
 * Create a binary heap from the array heap->root of length total.
 * Afterwards, set heap->length = heap->total.  This function uses
 * Floyd's method for heap construction.
 */
static void
build_heap(struct heap *heap)
{
	size_t i;

	heap->length = heap->total;

	/* remember, i is unsigned so i >= 0 is a tautology */
	for (i = heap->length / 2; i > 0; i--)
		heapify(heap, i - 1);
}

/*
 * Remove heap's root and then restore the heap property.
 */
static void
remove_root(struct heap *heap)
{

	exchange(heap, 0, --heap->length);
	heapify(heap, 0);
}

/*
 * Compare to struct vertex by the value of cmbindex in a manner
 * suitable for qosrt() and bsearch()
 */
static int
compare_by_index(const void *aarg, const void *barg)
{
	const struct vertex *a = aarg, *b = barg;

	return ((a->index > b->index) - (a->index < b->index));
}

/*
 * Compute the intersection of the neighborhood of the configuration
 * represented by cmb and the configurations in far.  Store pointers
 * to the configurations found in reach.  Return the number of
 * configurations found.  reach must provide space to store up to
 * REACH_LEN entries.
 *
 * TODO: Make this work with zero-aware PDBs.
 */
static size_t
compute_reach(tileset ts, struct vertex *reach[REACH_LEN], cmbindex cmb,
    struct vertex *far, size_t n_far)
{
	struct puzzle p;
	struct index idx;
	struct vertex key = { 0, 0 }, *loc;
	tileset eq, req;
	size_t n_reach = 0, i, zloc, n_move, least;
	const signed char *moves;

	split_index(ts, &idx, cmb);
	invert_index(ts, &p, &idx);
	eq = tileset_eqclass(ts, &p);
	zloc = zero_location(&p);

	// for (req = tileset_reduce_eqclass(eq); !tileset_empty(req); req = tileset_remove_least(req)) {
	for (req = eq; !tileset_empty(req); req = tileset_remove_least(req)) {
		least = tileset_get_least(req);
		n_move = move_count(least);
		moves = get_moves(least);
		move(&p, least);

		for (i = 0; i < n_move; i++) {
			if (tileset_has(eq, moves[i]))
				continue;

			move(&p, moves[i]);

			key.index = full_index(ts, &p);
			loc = bsearch(&key, far, n_far, sizeof *far, compare_by_index);
			if (loc != NULL && loc->additions != DOMINATED)
				reach[n_reach++] = loc;

			move(&p, least);
		}

		move(&p, zloc);
	}

	assert(n_reach <= REACH_LEN);

	// fprintf(stderr, "Index %llu reaches %zu entries.\n", cmb, n_reach);

	return (n_reach);
}

/*
 * Given two adjacent equidistance classes far and near of sizes n_far
 * and n_near, compute a subset of near that dominates all of far.  far
 * does not need to contain all elements in the equidistance class, but
 * only those elements of the equidistance class found in far are
 * considered. If any element is far is marked DOMINATED, behaviour is
 * undefined.  This function returns the number of elements in near
 * needed to cover far.  The elements selected are marked as DOMINATED,
 * the content of far[*].additions is destroyed.  Both arrays are
 * permuted as a result of this function.
 */
static size_t
find_dominating_set(tileset ts, struct vertex *far, size_t n_far,
    struct vertex *near, size_t n_near)
{
	struct heap heap;
	size_t i, n_dominatee = n_far, n_reach;
	struct vertex *root, *reach[REACH_LEN];

	/* make bsearch() work */
	qsort(far, n_far, sizeof *far, compare_by_index);

	for (i = 0; i < n_near; i++)
		near[i].additions = compute_reach(ts, reach, near[i].index, far, n_far);

	heap.root = near;
	heap.total = n_near;
	build_heap(&heap);

	while (n_dominatee > 0 && heap.length > 0) {
		root = get_root(&heap);
		n_reach = compute_reach(ts, reach, root->index, far, n_far);

		/*
		 * If some vertices root reaches were already dominated by
		 * previously added vertices in near, we need to decrement
		 * root->additions and potentially sift it down the
		 * heap.  If this changed anything about root being the
		 * heap's root, we need to try again.
		 */
		if (n_reach != root->additions) {
			root->additions = n_reach;
			if (!heapify(&heap, 0))
				continue;
		}

		/* we should never add a vertex that does not dominate anything new */
		assert(n_reach != 0);
		for (i = 0; i < n_reach; i++)
			reach[i]->additions = DOMINATED;

		// fprintf(stderr, "Removing index %llu dominating %zu entries.\n", root->index, n_reach);

		/* assumes that every reach[i] is a distinct element */
		n_dominatee -= n_reach;
		root->additions = DOMINATED;
		remove_root(&heap);
	}

	/*
	 * If we added every single vertex in near but haven't dominated
	 * far, something went terribly wrong.
	 */
	assert(n_dominatee == 0);

	return (heap.total - heap.length);
}

/*
 * Accumulate the indices of all puzzle configurations belonging into
 * the same equidistance class and store them in a freshly allocated
 * array.  n_eqdist must be equal to the number of members of the
 * equidistance class.  The additions field of each entry is set to 0.
 * If memory allocation fails, abort the program.
 *
 * TODO: Consider if there is a purpose in making this parallel.
 */
static struct vertex *
accumulate_eqclass(patterndb pdb, tileset ts, size_t distance, size_t n_eqdist)
{
	struct vertex *eqdist;
	size_t i, j, n = search_space_size(ts);

	eqdist = malloc(n_eqdist * sizeof * eqdist);
	if (eqdist == NULL) {
		perror("malloc");
		abort();
	}

	for (i = j = 0; i < n; i++)
		if (pdb[i] == distance) {
			eqdist[j].index = i;
			eqdist[j++].additions = 0;
		}

	return (eqdist);
}

/*
 * Eradicate the configurations not marked as DOMINATED in eqdist from the
 * pattern database.  Then move all entries not marked DOMINATED to the
 * front and return the number of entries not marked DOMINATED.
 */
static size_t
eradicate_entries(patterndb pdb, struct vertex *eqdist, size_t n_eqdist)
{
	size_t i, j;

	/* invariant: j <= i */
	for (i = j = 0; i < n_eqdist; i++)
		if (eqdist[i].additions != DOMINATED) {
			pdb[eqdist[i].index] = INFINITY;
			eqdist[j++] = eqdist[i];
		}

	return (j);
}

/*
 * This module reduces the number of configurations in a pattern
 * database by computing a small dominating set of the configurations
 * such that each configuration is either in the dominating set or
 * directly connected to (dominated by) a configuration in the set
 * whose distance is one lower than the distance of the dominated
 * configuration.
 *
 * It is well known that the dominating set problem is NP-hard and
 * inapproximatable beyond a factor a logarithmic factor in the
 * maximum degree.  Since the pattern databases tend to grow quite
 * large, the standard greedy algorithm is infeasable.  Observing
 * that the quotient graph under equidistance from the solved
 * configuration is a line graph, we consider one pair of adjacent
 * equivalence classes at a time starting from the most distant
 * equivalence classes and find a subset of the lower-ranked class
 * dominating all higher-ranked vertices not already in the dominating
 * set and add it to our dominating set.
 *
 * In this implementation, pdb is modified in place.  The code is
 * not parallelized except for some subroutines.  Status information
 * is written to f when f is not NULL.
 */
extern void
reduce_patterndb(patterndb pdb, tileset ts, FILE *f)
{
	cmbindex histogram[PDB_HISTOGRAM_LEN];
	struct vertex *far, *near;
	size_t n_classes, i, n_far, n_near;

	n_classes = generate_pdb_histogram(histogram, pdb, ts);
	if (n_classes < 2)
		return;

	if (f != NULL)
		fprintf(f, "Histogram: %zu classes.\n\n", n_classes);

	n_far = histogram[n_classes - 1];
	far = accumulate_eqclass(pdb, ts, n_classes - 1, n_far);

	/* remove all entries in the farthest equivalence class from the PDB */
	for (i = 0; i < n_far; i++)
		pdb[far[i].index] = INFINITY;

	if (f != NULL)
		fprintf(f, "%3zu: %20zu/%20zu (%5.2f%%)\n", n_classes - 1, (size_t)0, n_far, 0.0);

	for (i = n_classes - 1; i > 0; i--) {
		n_near = histogram[i - 1];
		near = accumulate_eqclass(pdb, ts, i - 1, n_near);
		find_dominating_set(ts, far, n_far, near, n_near);
		free(far);

		n_far = eradicate_entries(pdb, near, n_near);
		/* n_far <=  n_near should hold here and realloc should succeed */
		assert (n_far <= n_near);
		far = realloc(near, n_far * sizeof *far);
		assert(far != NULL);

		if (f != NULL)
			fprintf(f, "%3zu: %20zu/%20zu (%5.2f%%)\n", i - 1,
			    (n_near - n_far), n_near, (100.0 * (n_near - n_far)) / n_near);
	}

	free(far);
}
