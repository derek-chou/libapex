CC = gcc
INC = -I .
CFLAGS = -Wall
LDFLAGS = -lpthread

test: test.o tw_tick.o dyn_array.o
	$(CC) -o test test.o tw_tick.o dyn_array.o $(INC) $(CFLAGS) $(LDFLAGS)
test.o: test.c tw_tick.h
	$(CC) test.c $(CFLAGS) -c
tw_tick.o: tw_tick.c
	$(CC) tw_tick.c $(CFLAGS) -c
dyn_array.o: dyn_array.c
	$(CC) dyn_array.c $(CFLAGS) -c
clean:
	@rm -rf *.o
