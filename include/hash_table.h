/* hash_table.h */
#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <error.h>
#include <stdbool.h>

#include "utils.h"
#include "memora.h"

#define LOAD_FACTOR_THRESHOLD 0.7

struct s_entry {
    int8 *key;
    int8 *value;
    struct s_entry *next;
    struct s_entry *prev;
};
    
typedef struct s_entry Entry;

struct s_hash_table {
    Entry **buckets; // Now a dynamic array
    int32 capacity; // Number of buckets
    int32 count; // Number of entries
};

typedef struct s_hash_table HashTable;

// The single global hash table for the active db, mirroring `root` in tree.h.
extern HashTable *hash_table;

int32 hash(int8 *key, int32 capacity);
HashTable *create_table(int32 capacity);
void free_table(HashTable *hash_table);
int16 resize_table(HashTable *hash_table, int32 capacity);
int8 *get_value(HashTable *hash_table, int8 *key);
void print_hash_table(Client *client, HashTable *hash_table);
Entry *get_entry(HashTable *hash_table, int8 *key);
int16 exists(HashTable *hash_table, int8 *key);
int16 insert(HashTable *hash_table, int8 *key, int8 *value);
int16 delete(HashTable *hash_table, int8 *key);

#endif
