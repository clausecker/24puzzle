/* catalogue.h -- pattern database catalogues */

#include <stdio.h>

#include "pdb.h"
#include "tileset.h"
#include "puzzle.h"

/*
 * A struct pdb_catalogue stores a catalogue of pattern databases.
 * Groups of pattern databases are used to form distance heuristics, a
 * PDB catalogue stores multiple such groups and can compute the maximal
 * h value of all groups.  The member pdbs contains the pattern
 * databases we are interested in.  The member parts contains a bitmap
 * of which PDBs make up which heuristic.  The member heuristics
 * contains a bitmap of which heuristics each PDB is used for.  The
 * member pdbs_ts contains for the PDB's tile sets for better cache
 * locality.
 */
enum {
	CATALOGUE_PDBS_LEN = 64,
	HEURISTICS_LEN = 16,
};

struct pdb_catalogue {
	struct patterndb *pdbs[CATALOGUE_PDBS_LEN];
	unsigned heuristics[CATALOGUE_PDBS_LEN];
	tileset pdbs_ts[CATALOGUE_PDBS_LEN];
	unsigned long long parts[HEURISTICS_LEN];
	size_t n_pdbs, n_heuristics;
};

/*
 * A struct partial_hvals stores the partial h values of a puzzle
 * configuration for the PDBs in a PDB catalogue.  This is useful so we
 * can avoid superfluous PDB lookups by not looking up values that did
 * not change change whenever we can.  The member fake_entries stores a
 * bitmap of those PDB whose entries we have not bothered to look up as
 * they do not contribute to the best heuristic for this puzzle
 * configuration.  The member maximums stores the entries that made up
 * the maximal h values.
 */
struct partial_hvals {
	unsigned char hvals[CATALOGUE_PDBS_LEN];
	unsigned long long fake_entries, maximums;
};

extern struct pdb_catalogue	*catalogue_load(const char *, const char *, FILE *);
extern unsigned	catalogue_hval(struct pdb_catalogue *, const struct puzzle *);
extern unsigned	catalogue_partial_hvals(struct partial_hvals *, struct pdb_catalogue *, const struct puzzle *);
extern unsigned	catalogue_diff_hvals(struct partial_hvals *, struct pdb_catalogue *, const struct puzzle *, unsigned);
