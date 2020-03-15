cvm:
	mkdir -p bin
	gcc -Wl,--export-dynamic -g -o bin/cvm cvm.c -ldl

chasm:
	mkdir -p bin
	yacc -d chasm.yacc
	lex chasm.lex
	gcc y.tab.c lex.yy.c -I/usr/local/include -o bin/chasm

all: chasm cvm
