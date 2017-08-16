CC=clang
CFLAGS=-msse4.2 -mpopcnt -O3 -Wall -Wno-missing-braces -Wno-parentheses -I. -g
LDLIBS=-lpthread

OBJ=index.o puzzle.o tileset.o validation.o pdbgen.o pdbverify.o parallel.o \
	histogram.o
BINARIES=test/indextest test/tiletest cmd/genpdb cmd/verifypdb

all: $(BINARIES) 24puzzle.a

.o:
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

24puzzle.a: $(OBJ)
	ar rc $@ $?

test/indextest: test/indextest.o 24puzzle.a
test/tiletest: test/tiletest.o 24puzzle.a
cmd/genpdb: cmd/genpdb.o 24puzzle.a
cmd/verifypdb: cmd/verifypdb.o 24puzzle.a

clean:
	rm -f *.a *.o test/*.o cmd/*.o $(BINARIES)

.PHONY: all clean
