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

void insert_table(Table *t, int key, struct sockaddr *val)
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
    newNode->next = list;
    t->list[pos] = newNode;
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
            free(temp->val);
            free(temp);
            return;
        }
        pre_temp = temp;
        temp = temp->next;
    }
}
