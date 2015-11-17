cflags= -Wall -std=c11

.PHONY: build clean run all

all: run

http.o: http.c 
	gcc -c $(cflags) http.c

worker.o: worker.c 
	gcc -c $(cflags) worker.c 

worker: worker.o
	gcc -o worker $(cflags) worker.o -lnanomsg

http: http.o
	gcc -o http $(cflags) http.o -lnanomsg -lmill -luuid

clean:
	rm *.o

run: worker http
	./http
