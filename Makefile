CC=clang
CFLAGS=-msse4.2 -mpopcnt -O3 -Wall -Wno-missing-braces -I.

OBJ=index.o puzzle.o tileset.o validation.o pdbgen.o
BINARIES=test/indextest

all: $(OBJ)

test/indextest: $(OBJ) test/indextest.o
	$(CC) $(LDFLAGS) -o $@ $(OBJ) test/indextest.o

clean:
	rm -f *.o test/*.o $(BINARIES)

.PHONY: all clean
