CC=clang
CFLAGS=-march=native -O3 -Wall -Wno-missing-braces -Wno-parentheses -I. -g
LDLIBS=-lpthread

#OBJ=index.o puzzle.o tileset.o validation.o pdbgen.o pdbverify.o parallel.o \
#	histogram.o pdbdom.o ranktbl.o rank.o
#BINARIES=test/indextest test/tiletest cmd/genpdb cmd/verifypdb cmd/reducepdb \
#	cmd/pdbstats util/rankgen test/ranktest
OBJ=index.o puzzle.o tileset.o validation.o ranktbl.o rank.o random.o pdb.o \
	moves.o parallel.o
BINARIES=cmd/pdbstats test/indextest test/tiletest util/rankgen test/ranktest

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
cmd/genpdb: cmd/genpdb.o 24puzzle.a
cmd/verifypdb: cmd/verifypdb.o 24puzzle.a
cmd/reducepdb: cmd/reducepdb.o 24puzzle.a
cmd/pdbstats: cmd/pdbstats.o 24puzzle.a
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS) -lm

clean:
	rm -f *.a *.o test/*.o cmd/*.o util/*.o ranktbl.c $(BINARIES)

.PHONY: all clean
