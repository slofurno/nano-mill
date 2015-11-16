
all: run

build:
	gcc -o http http.c -Wall -std=c11 -lmill

run: build
	./http
