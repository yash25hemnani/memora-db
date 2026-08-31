/* hash_table.h */
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