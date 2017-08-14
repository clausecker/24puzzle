CC=clang
CFLAGS=-msse4.2 -mpopcnt -O3 -Wall -Wno-missing-braces -Wno-parentheses -I. -g
LDLIBS=-lpthread

OBJ=index.o puzzle.o tileset.o validation.o pdbgen.o pdbverify.o
BINARIES=test/indextest test/tiletest cmd/genpdb

all: $(BINARIES) $(OBJ)

test/indextest: $(OBJ) test/indextest.o
	$(CC) $(LDFLAGS) -o $@ $(OBJ) test/indextest.o $(LDLIBS)

test/tiletest: $(OBJ) test/tiletest.o
	$(CC) $(LDFLAGS) -o $@ $(OBJ) test/tiletest.o $(LDLIBS)

cmd/genpdb: $(OBJ) cmd/genpdb.o
	$(CC) $(LDFLAGS) -o $@ $(OBJ) cmd/genpdb.o $(LDLIBS)

clean:
	rm -f *.o test/*.o cmd/*.o $(BINARIES)

.PHONY: all clean
