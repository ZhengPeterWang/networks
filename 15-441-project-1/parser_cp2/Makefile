parser: y.tab.o lex.yy.o
	gcc -g $^ -o parser

lex.yy.c: lexer.l
	flex $^

%.o: %.c
	gcc -g -c $^ -o $@

y.tab.c: parser.y
	yacc -d $^

clean:
	rm -f *.o http lex.yy.c y.tab.c y.tab.h parser http-scanner parser.tar

handin: lexer.l parser.y Makefile http-request.txt README
	tar -cvf parser.tar $^
