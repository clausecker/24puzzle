#!/bin/sh

set -e

# generate samples using cmd/sphericalsamples
NPROC=20
pdbdir=/local/bzcclaus/pdbcat
fsm=/local/bzcclaus/loops/22.mfsm
sampledir=/local/bzcclaus/spheresamples
catalogue=catalogues/small-t.cat
nsamples=10000000
seed=8528
start=28
limit=55
cmddir=cmd
reportfile=$sampledir/report

PATH="$cmddir:$PATH"

# are we a worker?
if [ "$1" = -w ]
then
	dist="$2"

	nice spheresample -r -d "$pdbdir" -m "$fsm" -n "$nsamples" \
	    -o "$sampledir/$dist.sample" -s "`printf $seed%02d $dist`" \
	    "$catalogue" "$dist" | tee -a "$reportfile"

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

mkdir -p "$sampledir"
seq $start $limit | xargs -P "$NPROC" -n 1 -- "$0" -w
