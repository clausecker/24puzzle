#!/bin/sh

set -e

# this script generates all PDBs of the given size, 6 by default and
# stores them to the given directory.
tiles=6
pdbdir=/local/bzcclaus/scratch/pdbs
cmddir=.
datefmt="%Y-%m-%d %T:"
qualities="$pdbdir/qualities.txt"
histograms="$pdbdir/histograms.txt"

PATH="$cmddir:$PATH"

# are we a worker?
if [ "$1" = -w ]
then
	ts="$2"
	pdbstem="$pdbdir/$ts"

	# has this already been generated?
	if [ -e "$pdbstem.bpdb.zst" ] && zstd -qq -t "$pdbstem.bpdb.zst"
	then
		echo `date "+$datefmt"` "$ts" already present
		exit
	fi

	genpdb -q -j 1 -t "$ts" -f "$pdbstem.pdb"
	pdbquality -j 1 -t "$ts" "$pdbstem.pdb" >>$qualities
	pdbstats -p -t "$ts" "$pdbstem.pdb" >>$histograms
	bitpdb -t "$ts" -o "$pdbstem.bpdb" "$pdbstem.pdb"
	rm -f "$pdbstem.pdb"
	zstd -q --rm --ultra -22 -f "$pdbstem.bpdb"
	echo `date "+$datefmt"` "$ts" generated
	exit
fi

# find number of CPUs to use
if [ ! "$NPROC" ]
then
	case `uname` in
	Linux) NPROC=`nproc` ;;
	Darwin|FreeBSD) NPROC=`sysctl -n hw.ncpu` ;;
	*) NPROC=1 ;;
	esac
fi

echo `date "+$datefmt"` Generating all PDBs for "$tiles" tiles to "$pdbdir"
mkdir -p "$pdbdir"
pdbcount -z -p "$tiles" | xargs -P "$NPROC" -n 1 -- "$0" -w
echo `date "+$datefmt"` Finished generating all PDBs for "$tiles" tiles to "$pdbdir"
