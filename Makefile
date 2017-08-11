CC=clang
CFLAGS=-msse4.2 -mpopcnt -O3 -Wall -Wno-missing-braces -Wno-parentheses -I. -g

OBJ=index.o puzzle.o tileset.o validation.o pdbgen.o
BINARIES=test/indextest test/tiletest cmd/genpdb

all: $(BINARIES) $(OBJ)

test/indextest: $(OBJ) test/indextest.o
	$(CC) $(LDFLAGS) -o $@ $(OBJ) test/indextest.o

test/tiletest: $(OBJ) test/tiletest.o
	$(CC) $(LDFLAGS) -o $@ $(OBJ) test/tiletest.o

cmd/genpdb: $(OBJ) cmd/genpdb.o
	$(CC) $(LDFLAGS) -o $@ $(OBJ) cmd/genpdb.o

clean:
	rm -f *.o test/*.o cmd/*.o $(BINARIES)

.PHONY: all clean
