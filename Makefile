all:
	mkdir -p bin
	cc -g -o bin/cvm cvm.h cvm.c
