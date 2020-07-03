#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/ip.h>

typedef struct Node
{
    int key;
    struct sockaddr *val;
    struct Node *next;
} Node;

typedef struct
{
    int size;
    Node **list;
} Table;

Table *create_table(int size);

void insert_table(Table *t, int key, struct sockaddr *val);

struct sockaddr *lookup_table(Table *t, int key);

void remove_table(Table *t, int key);
