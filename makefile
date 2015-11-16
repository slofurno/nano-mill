cflags= -Wall -std=c11
prefix= /usr/local

all: run

http.o: 
	gcc -c $(cflags) http.c

publish.o: 
	gcc -c $(cflags) publish.c 

build: http.o publish.o
	gcc $(cflags) http.c -o http -lmill
	gcc publish.c -o publish $(cflags) -lnanomsg -lmill

clean:
	rm *.o

run: build
	./http
