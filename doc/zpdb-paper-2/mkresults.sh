#!/bin/sh

# a script to create the result files.  Must be executed in the
# top directory of the 24puzzle repository.

set -e
j=`nproc`
pdbdir="`mktemp -d -t pdbs.XXXXXX`"
results=doc/zpdb-paper-2

if [ ! -f Makefile ]
then
	echo Please execute me from the top of the 24puzzle repository.
	exit 1
fi

make clean
make all

mkdir -p -- "$results"

# two separate loops so we only need to keep one set of
# PDBs in $pdbdir at a time.
for cat in small reinefeld clausecker
do
	cmd/parsearch    -j $j -d "$pdbdir" catalogues/$cat-t.cat doc/korf-raw.txt >"$results"/korf-$cat-zpdb.out
done

rm -f -- "$pdbdir/*"

for cat in small reinefeld clausecker
do
	cmd/parsearch -i -j $j -d "$pdbdir" catalogues/$cat-t.cat doc/korf-raw.txt >"$results"/korf-$cat-apdb.out
done

rm -f -- "$pdbdir/*"
rmdir -- "$pdbdir"
