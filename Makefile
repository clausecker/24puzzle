CC=clang
CFLAGS=-march=native -O3 -g
COPTS=-std=c11 -I. -Wall -Wno-missing-braces -Wno-parentheses
HOSTCC=cc
HOSTCFLAGS=-O3 -g
HOSTCOPTS=-w -I. $(HOSTCFLAGS)
LDLIBS=-lpthread -lzstd -lm

ZSTDCOPTS=-I$(HOME)/include
ZSTDLDFLAGS=-L$(HOME)/lib
ZSTDLDLIBS=$(HOME)/lib/libzstd.a

OBJ=index.o puzzle.o tileset.o validation.o ranktbl.o rank.o random.o pdb.o \
	moves.o parallel.o pdbgen.o pdbverify.o pdbdiff.o \
	ida.o search.o catalogue.o pdbident.o transposition.o \
	heuristic.o bitpdb.o bitpdbzstd.o match.o quality.o compact.o \
	statistics.o fsm.o fsmwrite.o

BINARIES=cmd/pdbstats test/indextest util/rankgen test/ranktest cmd/genpdb \
	cmd/verifypdb cmd/bitpdb cmd/diffcode test/rankcount cmd/puzzlegen \
	test/qualitytest test/hitanalysis cmd/parsearch cmd/pdbsearch \
	cmd/pdbcount test/bitpdbtest test/morphtest cmd/pdbmatch \
	cmd/pdbquality test/walkdist cmd/puzzledist test/etatest \
	test/samplegen test/statmerge cmd/etacount cmd/randompdb cmd/genloops \
	cmd/compilefsm test/explore test/indexbench cmd/spheresample \
	cmd/addmoribund cmd/sampleeta test/expansions

all: $(BINARIES) 24puzzle.a

size: $(BINARIES) 24puzzle.a
	@size $(BINARIES) 24puzzle.a

.o:
	@echo "CCLD	$@"
	@$(CC) $(ZSTDLDFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

24puzzle.a: $(OBJ)
	@echo "AR	$@"
	@ar -src $@ $?

util/rankgen: util/rankgen.c
	@echo "HOSTCC  $<"
	@$(HOSTCC) $(HOSTCOPTS) -o $@ $<

ranktbl.c: util/rankgen
	@echo "RANKGEN	$@"
	@util/rankgen >$@

test/hitanalysis: test/hitanalysis.o 24puzzle.a
test/indexbench: test/indexbench.o 24puzzle.a
test/indextest: test/indextest.o 24puzzle.a
test/tiletest: test/tiletest.o 24puzzle.a
cmd/addmoribund: cmd/addmoribund.o 24puzzle.a
cmd/parsearch: cmd/parsearch.o 24puzzle.a
test/ranktest: test/ranktest.o 24puzzle.a
test/qualitytest: test/qualitytest.o 24puzzle.a
cmd/genpdb: cmd/genpdb.o 24puzzle.a
cmd/verifypdb: cmd/verifypdb.o 24puzzle.a
cmd/diffcode: cmd/diffcode.o 24puzzle.a
test/rankcount: test/rankcount.o 24puzzle.a
cmd/bitpdb: cmd/bitpdb.o 24puzzle.a
cmd/compilefsm: cmd/compilefsm.o 24puzzle.a
cmd/etacount: cmd/etacount.o 24puzzle.a
cmd/genloops: cmd/genloops.o 24puzzle.a
cmd/pdbstats: cmd/pdbstats.o 24puzzle.a
cmd/pdbsearch: cmd/pdbsearch.o 24puzzle.a
cmd/puzzledist: cmd/puzzledist.o 24puzzle.a
cmd/puzzlegen: cmd/puzzlegen.o 24puzzle.a
cmd/pdbcount: cmd/pdbcount.o 24puzzle.a
cmd/pdbmatch: cmd/pdbmatch.o 24puzzle.a
cmd/pdbquality: cmd/pdbquality.o 24puzzle.a
cmd/sampleeta: cmd/sampleeta.o 24puzzle.a
cmd/spheresample: cmd/spheresample.o 24puzzle.a
cmd/randompdb: cmd/randompdb.o 24puzzle.a
test/bitpdbtest: test/bitpdbtest.o 24puzzle.a
test/morphtest: test/morphtest.o 24puzzle.a
test/walkdist: test/walkdist.o 24puzzle.a
test/etatest: test/etatest.o 24puzzle.a
test/expansions: test/expansions.o 24puzzle.a
test/explore: test/explore.o 24puzzle.a
test/samplegen: test/samplegen.o 24puzzle.a
test/statmerge: test/statmerge.o 24puzzle.a

.c.o:
	@echo "CC	$<"
	@$(CC) $(ZSTDCOPTS) $(COPTS) $(CFLAGS) -c -o $@ $<

clean:
	@echo "CLEAN"
	@rm -f *.a *.o test/*.o cmd/*.o util/*.o ranktbl.c $(BINARIES)

.PHONY: all clean size
