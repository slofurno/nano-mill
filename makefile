cflags= -Wall -std=c11

all: run

http.o: 
	gcc -c $(cflags) http.c

worker.o: 
	gcc -c $(cflags) worker.c 

build: http.o worker.o
	gcc $(cflags) http.o -o http -lnanomsg -lmill -luuid
	gcc $(cflags) worker.o -o worker -lnanomsg

clean:
	rm *.o

run: build
	./http
