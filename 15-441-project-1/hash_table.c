#include "hash_table.h"

Table *create_table(int size)
{
    Table *t = (Table *)malloc(sizeof(Table));
    t->size = size;
    t->list = (Node **)malloc(size * sizeof(Node *));
    for (int i = 0; i < size; ++i)
    {
        t->list[i] = NULL;
    }
    return t;
}

int hashCode(Table *t, int key)
{
    if (key < 0)
        return -(key % t->size);
    return key % t->size;
}

int hashCodeMap(Map *map, int key)
{
    if (key < 0)
        return -(key % map->size);
    return key % map->size;
}

void insert_table(Table *t, int key, struct sockaddr *val, int connection)
{
    int pos = hashCode(t, key);
    Node *list = t->list[pos];
    Node *temp = list;
    while (temp)
    {
        if (temp->key == key)
        {
            temp->val = val;
            return;
        }
        temp = temp->next;
    }
    Node *newNode = (Node *)malloc(sizeof(Node));
    newNode->key = key;
    newNode->val = val;
    newNode->connection = connection;
    newNode->next = list;
    newNode->is_cgi = 0;
    newNode->client_context = NULL;
    t->list[pos] = newNode;
}

void insert_table_with_context(Table *t, int key, struct sockaddr *val, int connection, SSL *client_context)
{
    int pos = hashCode(t, key);
    Node *list = t->list[pos];
    Node *temp = list;
    while (temp)
    {
        if (temp->key == key)
        {
            temp->val = val;
            return;
        }
        temp = temp->next;
    }
    Node *newNode = (Node *)malloc(sizeof(Node));
    newNode->key = key;
    newNode->val = val;
    newNode->connection = connection;
    newNode->next = list;
    newNode->is_cgi = 0;
    newNode->client_context = client_context;
    t->list[pos] = newNode;
}

void insert_cgi(Table *t, int key, int num)
{
    int pos = hashCode(t, key);
    Node *list = t->list[pos];
    Node *temp = list;
    while (temp)
    {
        if (temp->key == key)
        {
            temp->is_cgi = num;
            return;
        }
        temp = temp->next;
    }
}

struct sockaddr *lookup_table(Table *t, int key)
{
    int pos = hashCode(t, key);
    Node *list = t->list[pos];
    Node *temp = list;
    while (temp)
    {
        if (temp->key == key)
        {
            return temp->val;
        }
        temp = temp->next;
    }
    return NULL;
}

int lookup_table_connection(Table *t, int key)
{
    int pos = hashCode(t, key);
    Node *list = t->list[pos];
    Node *temp = list;
    while (temp)
    {
        if (temp->key == key)
        {
            return temp->connection;
        }
        temp = temp->next;
    }
    return -1;
}

int lookup_table_cgi(Table *t, int key)
{
    int pos = hashCode(t, key);
    Node *list = t->list[pos];
    Node *temp = list;
    while (temp)
    {
        if (temp->key == key)
        {
            return temp->is_cgi;
        }
        temp = temp->next;
    }
    return -1;
}

SSL *lookup_table_context(Table *t, int key)
{
    int pos = hashCode(t, key);
    Node *list = t->list[pos];
    Node *temp = list;
    while (temp)
    {
        if (temp->key == key)
        {
            return temp->client_context;
        }
        temp = temp->next;
    }
    return NULL;
}

void remove_table(Table *t, int key)
{
    int pos = hashCode(t, key);

    Node *oldNode = t->list[pos];

    Node *temp = oldNode;
    Node *pre_temp = NULL;
    while (temp)
    {
        if (temp->key == key)
        {
            if (pre_temp != NULL)
                pre_temp->next = temp->next;
            else
                t->list[pos] = temp->next;
            free(temp->val);
            if (temp->client_context != NULL)
            {
                SSL_shutdown(temp->client_context);
                SSL_free(temp->client_context);
            }
            free(temp);
            return;
        }
        pre_temp = temp;
        temp = temp->next;
    }
}

void remove_all_entries_in_table(Table *t)
{
    for (int i = 0; i < t->size; ++i)
    {
        if (t->list[i] == NULL)
            continue;
        while (t->list[i] != NULL)
        {
            remove_table(t, t->list[i]->key);
        }
    }
    free(t->list);
    free(t);
}

Map *create_map(int size)
{
    Map *t = (Map *)malloc(sizeof(Map));
    t->size = size;
    t->list = (Map_Node **)malloc(size * sizeof(Map_Node *));
    for (int i = 0; i < size; ++i)
    {
        t->list[i] = NULL;
    }
    return t;
}

void insert_map(Map *map, int key, int sock)
{
    int pos = hashCodeMap(map, key);
    Map_Node *list = map->list[pos];
    Map_Node *temp = list;
    while (temp)
    {
        if (temp->key == key)
        {
            temp->sock = sock;
            return;
        }
        temp = temp->next;
    }
    Map_Node *newNode = (Map_Node *)malloc(sizeof(Map_Node));
    newNode->key = key;
    newNode->sock = sock;

    map->list[pos] = newNode;
}

int lookup_map(Map *map, int key)
{
    int pos = hashCodeMap(map, key);
    Map_Node *list = map->list[pos];
    Map_Node *temp = list;
    while (temp)
    {
        if (temp->key == key)
        {
            return temp->sock;
        }
        temp = temp->next;
    }
    return -1;
}

void remove_map(Map *map, int key)
{
    int pos = hashCodeMap(map, key);

    Map_Node *oldNode = map->list[pos];

    Map_Node *temp = oldNode;
    Map_Node *pre_temp = NULL;
    while (temp)
    {
        if (temp->key == key)
        {
            if (pre_temp != NULL)
                pre_temp->next = temp->next;
            else
                map->list[pos] = temp->next;
            free(temp);
            return;
        }
        pre_temp = temp;
        temp = temp->next;
    }
}

void destroy_map(Map *map)
{
    for (int i = 0; i < map->size; ++i)
    {
        if (map->list[i] == NULL)
            continue;
        while (map->list[i] != NULL)
        {
            remove_map(map, map->list[i]->key);
        }
    }
    free(map->list);
    free(map);
}