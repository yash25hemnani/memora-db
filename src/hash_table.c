/* hash_table.c */
#include "hash_table.h"

// For our use case, we will store path as key and node address as value

int32 hash(int8 *key, int32 capacity)
{
    // We will do rolling polynomial hashing, return answer divided by capactiy to get proper array range
    int32 p = 31;
    int32 sum = 0;

    while (*key)
    {
        sum = sum * p + (int32)*key;
        key++;
    }

    return sum % capacity;
}

HashTable *create_table(int32 capacity)
{
    // Dynamically allocate memory and return a pointer to the table
    HashTable *hash_table = malloc(sizeof(HashTable));

    if (hash_table == NULL)
        return NULL;

    hash_table->buckets = malloc(sizeof(Entry *) * capacity);

    hash_table->capacity = capacity;
    hash_table->count = 0;

    return hash_table;
}

int8 *get_value(HashTable *hash_table, int8 *key)
{
    // Gets you the index in the bucket
    int32 index = hash(key, hash_table->capacity);

    Entry *entry = hash_table->buckets[index];

    while (entry)
    {
        if (strcmp((char *)entry->key, (char *)key) == 0)
        {
            return entry->value;
        }
    }

    return NULL;
}

Entry *get_entry(HashTable *hash_table, int8 *key)
{
    // Gets you the index in the bucket
    int32 index = hash(key, hash_table->capacity);

    Entry *entry = hash_table->buckets[index];

    while (entry)
    {
        if (strcmp((char *)entry->key, (char *)key) == 0)
        {
            return entry;
        }
    }

    return NULL;
}

int16 exists(HashTable *hash_table, int8 *key)
{
    Entry *entry = get_entry(hash_table, key);

    return entry ? STATUS_OK : STATUS_FALSE;
}

int16 insert(HashTable *hash_table, int8 *key, int8 *value)
{
    if (exists(hash_table, key))
    {
        return STATUS_ERROR; // Already exists
    }

    int32 index = hash(key, hash_table->capacity);

    Entry *entry = hash_table->buckets[index];

    // Bucket is empty
    if (entry == NULL)
    {
        Entry *new_entry = malloc(sizeof(Entry));

        if (new_entry == NULL)
        {
            return STATUS_ERROR; // Allocation failed
        }

        new_entry->key = key;
        new_entry->value = value;
        new_entry->next = NULL;
        new_entry->prev = NULL;

        hash_table->buckets[index] = new_entry;

        return STATUS_OK;
    }

    // Bucket already contains entries
    while (entry->next != NULL)
    {
        entry = entry->next;
    }

    Entry *new_entry = malloc(sizeof(Entry));

    if (new_entry == NULL)
    {
        return STATUS_ERROR;
    }

    new_entry->key = key;
    new_entry->value = value;
    new_entry->next = NULL;
    new_entry->prev = entry;

    entry->next = new_entry;

    return STATUS_OK;
}

int16 delete(HashTable *hash_table, int8 *key)
{
    Entry *entry = get_entry(hash_table, key);

    if (!entry)
    {
        return STATUS_ERROR;
    }

    // If first entry
    if (entry->prev == NULL)
    {
        int32 index = hash(key, hash_table->capacity);

        hash_table->buckets[index] = entry->next;

        if (entry->next != NULL)
        {
            entry->next->prev = NULL;
        }
    }
    else if (entry->next == NULL)
    {
        // If last entry
        entry->prev->next = NULL;
    }
    else
    {
        // If in middle
        entry->prev->next = entry->next;
        entry->next->prev = entry->prev;
    }

    free(entry);

    return STATUS_OK;
}