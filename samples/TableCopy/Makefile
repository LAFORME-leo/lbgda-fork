CFLAGS = -Wall -g `pkg-config --cflags libgda-4.0`
LDFLAGS = `pkg-config --libs libgda-4.0`

all: table-copy table-copy-easier

table-copy: table-copy.c common.o
	$(CC) -o table-copy common.o table-copy.c $(CFLAGS) $(LDFLAGS)

table-copy-easier: table-copy-easier.c common.o
	$(CC) -o table-copy-easier common.o table-copy-easier.c $(CFLAGS) $(LDFLAGS)

common.o: common.h common.c
	$(CC) -c -o common.o common.c $(CFLAGS)

clean:
	rm -f *~
	rm -f *.o
	rm -f table-copy table-copy-easier
	rm -f copy.db
