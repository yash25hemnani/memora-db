/* hash_table.c */
#include "hash_table.h"
#include "memora.h"

HashTable *hash_table = NULL;

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

void print_entry_list(Client *client, Entry *entry)
{
    while (entry != NULL)
    {
        int8 addr_buf[32];
        snprintf((char *)addr_buf, sizeof(addr_buf), "%p", (void *)entry->value);

        write_to_client(client, (char *)entry->key);
        write_to_client(client, ":");
        write_to_client(client, (char *)addr_buf);

        if (entry->next)
        {
            write_to_client(client, "-->");
        }

        entry = entry->next;
    }
}

void print_hash_table(Client *client, HashTable *hash_table)
{
    // Walk from element one
    for (int32 i = 0; i < hash_table->capacity; i++)
    {
        int8 index_buf[8];
        snprintf((char *)index_buf, sizeof(index_buf), "%d - ", i + 1);
        write_to_client(client, (char *)index_buf);

        Entry *entry = hash_table->buckets[i];

        if (entry) {
            print_entry_list(client, entry);
        } else {
            write_to_client(client, "NULL");
        }
        
        write_to_client(client, "\n");

    }
}

HashTable *create_table(int32 capacity)
{
    // Dynamically allocate memory and return a pointer to the table
    HashTable *hash_table = malloc(sizeof(HashTable));

    if (hash_table == NULL)
        return NULL;

    hash_table->buckets = malloc(sizeof(Entry *) * capacity);
    zero(hash_table->buckets, sizeof(Entry *) * capacity);

    hash_table->capacity = capacity;
    hash_table->count = 0;

    return hash_table;
}

void free_table(HashTable *hash_table)
{
    if (hash_table == NULL)
        return;

    for (int32 i = 0; i < hash_table->capacity; i++)
    {
        Entry *entry = hash_table->buckets[i];

        while (entry)
        {
            Entry *next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }

    free(hash_table->buckets);
    free(hash_table);
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

        entry = entry->next;
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

        entry = entry->next;
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

        new_entry->key = (int8 *)strdup((char *)key);
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

    new_entry->key = (int8 *)strdup((char *)key);
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

    // Free the strdup'd key along with the entry
    free(entry->key);
    free(entry);

    return STATUS_OK;
}