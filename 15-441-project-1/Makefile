CC=gcc
CFLAGS=-I. -g
DEPS = parse.h y.tab.h log.h hash_table.h
OBJ = y.tab.o lex.yy.o parse.o log.o hash_table.o lisod.o # echo_server.o 
FLAGS = -g -Wall

default:all

all: lisod echo_client

lex.yy.c: lexer.l
	flex $^

y.tab.c: parser.y
	yacc -d $^

%.o: %.c $(DEPS)
	$(CC) $(FLAGS) -c -o $@ $< $(CFLAGS)

# echo_server: $(OBJ)
# 	$(CC) -o $@ $^ $(CFLAGS) $(FLAGS)

lisod: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(FLAGS)

echo_client:
	$(CC) echo_client.c -o echo_client -Wall -Werror

clean:
	rm -f *~ *.o *.log example lex.yy.c y.tab.c y.tab.h echo_client lisod
	# echo_server
