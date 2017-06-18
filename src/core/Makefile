CC=gcc
LD=gcc
CFLAGS=-lconfig -lwebsockets -lpthread

main: config.o  ws.o
	$(CC) $(CFLAGS) -o ../../SleeperCel main.c config.o
clean:
	rm -f ../../SleeperCel *.o
