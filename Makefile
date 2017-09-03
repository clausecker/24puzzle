CC=clang
CFLAGS=-std=c11 -march=native -O3 -Wall -Wno-missing-braces -Wno-parentheses -I. -g
LDLIBS=-lpthread

OBJ=index.o puzzle.o tileset.o validation.o ranktbl.o rank.o random.o pdb.o \
	moves.o parallel.o pdbgen.o pdbverify.o pdbdiff.o histogram.o \
	cindex.o pdbdom.o
BINARIES=cmd/pdbstats test/indextest util/rankgen test/ranktest \
	cmd/genpdb cmd/verifypdb cmd/diffcode cmd/reducepdb test/qualitytest

all: $(BINARIES) 24puzzle.a

.o:
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

24puzzle.a: $(OBJ)
	ar -src $@ $?

util/rankgen: util/rankgen.o
ranktbl.c: util/rankgen
	util/rankgen >$@

test/indextest: test/indextest.o 24puzzle.a
test/tiletest: test/tiletest.o 24puzzle.a
test/ranktest: test/ranktest.o 24puzzle.a
test/qualitytest: test/qualitytest.o 24puzzle.a
cmd/genpdb: cmd/genpdb.o 24puzzle.a
cmd/verifypdb: cmd/verifypdb.o 24puzzle.a
cmd/reducepdb: cmd/reducepdb.o 24puzzle.a
cmd/diffcode: cmd/diffcode.o 24puzzle.a
cmd/pdbstats: cmd/pdbstats.o 24puzzle.a
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) -lm

clean:
	rm -f *.a *.o test/*.o cmd/*.o util/*.o ranktbl.c $(BINARIES)

.PHONY: all clean
