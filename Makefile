chasm:
	mkdir -p bin
	yacc -d chasm.yacc
	lex chasm.lex
	gcc y.tab.c lex.yy.c -I/usr/local/include -o bin/chasm

all: chasm
	mkdir -p bin
	gcc -g -o bin/cvm cvm.c
