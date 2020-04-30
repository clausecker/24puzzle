#!/bin/sh

set -e

# generate samples using cmd/sphericalsamples
# NPROC=80
pdbdir=/local/bzcclaus/pdbcat
fsm=/local/bzcclaus/loops/24.mfsm
sampledir=/local/bzcclaus/spheresamples
catalogue=catalogues/small-t.cat
nsamples=100000000
nout=10000000
seed=4711
start=0
limit=69
cmddir=cmd
reportfile=$sampledir/report
statfile=$sampledir/stats

PATH="$cmddir:$PATH"

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
# puzzledist -l $((limit-1)) -f "$sampledir/" -n "$nsamples" | tee "$statfile"
for dist in `seq $start $limit`
do
	nice spheresample -r -d "$pdbdir" -m "$fsm" -n "$nsamples" \
	    -o "$sampledir/$dist.sample" -s "`printf $seed%02d $dist`" \
	    -N "$nout" -j "$NPROC" "$catalogue" "$dist" | tee -a "$reportfile"
done
