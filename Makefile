CC=clang
CFLAGS=-msse4.2 -mpopcnt -O3 -Wall -Wno-missing-braces -I.

OBJ=index.o puzzle.o tileset.o validation.o pdbgen.o
BINARIES=test/indextest cmd/genpdb.o

all: $(OBJ)

test/indextest: $(OBJ) test/indextest.o
	$(CC) $(LDFLAGS) -o $@ $(OBJ) test/indextest.o

cmd/genpdb: $(OBJ) cmd/genpdb.o
	$(CC) $(LDFLAGS) -o $@ $(OBJ) cmd/genpdb.o

clean:
	rm -f *.o test/*.o cmd/*.o $(BINARIES)

.PHONY: all clean
