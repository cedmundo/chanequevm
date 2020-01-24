all:
	mkdir -p bin
	cc -o bin/cvm cvm.h cvm.c
