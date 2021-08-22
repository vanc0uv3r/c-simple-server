CC=gcc

CFLAGS= -Wall -g

all: server

server: main.o
	$(CC) $(CFLAGS) main.o -o server
	rm -rf *.o

main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o main.o