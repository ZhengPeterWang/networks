#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <openssl/ssl.h>

typedef struct Node
{
    int key;
    int connection; // 0 HTTP, 1 HTTPS
    struct sockaddr *val;
    struct Node *next;
    SSL *client_context;
} Node;

typedef struct
{
    int size;
    Node **list;
} Table;

Table *create_table(int size);

void insert_table(Table *t, int key, struct sockaddr *val, int connection);

void insert_table_with_context(Table *t, int key, struct sockaddr *val, int connection, SSL *client_context);

struct sockaddr *lookup_table(Table *t, int key);

int lookup_table_connection(Table *t, int key);

SSL *lookup_table_context(Table *t, int key);

void remove_table(Table *t, int key);

void remove_all_entries_in_table(Table *t);
