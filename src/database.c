#include "database.h"
#include "memora.h"
#include "utils.h"
#include "users.h"
#include "tree.h"
#include "hash_table.h"
#include <sys/stat.h>

int16 add_ownership(Client *client, int8 *db_name)
{
    int8 db_path[256];
    snprintf((char *)db_path, sizeof(db_path), "%s/%s.db", DB_FOLDER, OWNERSHIP_DB);

    // Append to file
    FILE *file = fopen((char *)db_path, "a");

    if (file == NULL)
    {
        perror("fopen");
        return STATUS_ERROR;
    }

    fprintf(file, (char *)client->username);
    fprintf(file, "|");
    fprintf(file, (char *)db_name);
    fprintf(file, "\n");

    fclose(file);
    write_to_client(client, "OK: Ownership added successfully.\n");

    return STATUS_OK;
}

int16 check_ownership(Client *client, int8 *db_name)
{
    int8 db_path[256];
    snprintf((char *)db_path, sizeof(db_path), "%s/%s.db", DB_FOLDER, OWNERSHIP_DB);

    FILE *file = fopen((char *)db_path, "r");

    if (!file)
    {
        return STATUS_ERROR;
    }

    int8 line[256];
    int16 index = 1;

    while (fgets(line, sizeof(line), file))
    {
        Ownership ownership;

        sscanf(line, "%63[^|]|%127[^\n]", ownership.username, ownership.db_name);

        if (strcmp(ownership.db_name, db_name) == 0 && strcmp(ownership.username, client->username) == 0)
        {
            return STATUS_OK;
        }
    }

    return STATUS_FALSE;
}

int16 create_database(Client *client, int8 *db_name)
{
    // Check if file exists
    int8 db_path[256];
    int written = snprintf((char *)db_path, sizeof(db_path), "%s/%s.db", DB_FOLDER, (char *)db_name);

    if (written < 0 || (size_t)written >= sizeof(db_path))
    {
        write_to_client(client, "ERR: Database name too long.\n");
        return STATUS_ERROR;
    }

    // Create folder if it doesn't exist
    if (mkdir(DB_FOLDER, 0755) == -1 && errno != EEXIST)
    {
        perror("mkdir");
        return STATUS_ERROR;
    }

    // Check if database already exists
    FILE *file = fopen((char *)db_path, "r");

    if (file)
    {
        write_to_client(client, "ERR: Database already exists.\n");

        fclose(file);
        return STATUS_ERROR;
    }

    // Create the new database file
    file = fopen((char *)db_path, "w");

    if (file == NULL)
    {
        perror("fopen");
        return STATUS_ERROR;
    }

    fprintf(file, "/\n");

    // After creating, we will add to ownership file
    int16 ownership_added = add_ownership(client, db_name);

    if (ownership_added == 0)
    {
        write_to_client(client, "ERR: Failed to add ownership.\n");
        return STATUS_ERROR;
    }

    write_to_client(client, "OK: Database created successfully.\n");

    fclose(file);

    return STATUS_OK;
}

int16 delete_database(Client *client, int8 *db_name)
{
    int16 ownership = check_ownership(client, db_name);

    if (ownership != STATUS_OK)
    {
        write_to_client(client, "ERR: No such database exists.\n");
        return STATUS_ERROR;
    }

    int8 db_path[256];
    int written = snprintf((char *)db_path, sizeof(db_path), "%s/%s.db", DB_FOLDER, (char *)db_name);

    if (written < 0 || (size_t)written >= sizeof(db_path))
    {
        write_to_client(client, "ERR: Database name too long.\n");
        return STATUS_ERROR;
    }

    if (remove((char *)db_path) != 0)
    {
        perror("remove");
        return STATUS_ERROR;
    }

    write_to_client(client, "OK: Database deleted successfully.\n");

    return STATUS_OK;
}

int16 rename_database(Client *client, int8 *old_name, int8 *new_name)
{

    int16 ownership = check_ownership(client, old_name);

    if (ownership != STATUS_OK)
    {
        write_to_client(client, "ERR: No such database exists.\n");
        return STATUS_ERROR;
    }

    int8 old_path[256];
    int8 new_path[256];

    snprintf((char *)old_path, sizeof(old_path), "%s/%s.db", DB_FOLDER, (char *)old_name);
    snprintf((char *)new_path, sizeof(new_path), "%s/%s.db", DB_FOLDER, (char *)new_name);

    if (rename((char *)old_path, (char *)new_path) != 0)
    {
        perror("rename");
        return STATUS_ERROR;
    }

    write_to_client(client, "OK: Database renamed successfully.\n");

    return STATUS_OK;
}

void use_database(Client *client, int8 *db_name)
{
    int16 ownership = check_ownership(client, db_name);

    if (ownership != STATUS_OK)
    {
        write_to_client(client, "ERR: No such database exists.\n");
        return;
    }

    int8 db_path[256];
    snprintf((char *)db_path, sizeof(db_path), "%s/%s.db", DB_FOLDER, (char *)db_name);

    FILE *file = fopen((char *)db_path, "r");

    if (!file)
    {
        write_to_client(client, "ERR: No such database exists.\n");
        return;
    }

    strncpy(client->active_db, (char *)db_name, sizeof(client->active_db) - 1);
    client->active_db[sizeof(client->active_db) - 1] = '\0';

    reset_tree();
    hash_table = create_table(100);
    load_tree(client);

    int8 msg[256];
    snprintf((char *)msg, sizeof(msg), "OK: Using database '%s'.\n", db_name);
    write_to_client(client, (char *)msg);

    return;
}

void list_databases(Client *client)
{
    int8 db_path[256];
    snprintf((char *)db_path, sizeof(db_path), "%s/%s.db", DB_FOLDER, OWNERSHIP_DB);

    FILE *file = fopen((char *)db_path, "r");

    if (!file)
    {
        write_to_client(client, "ERR: No databases found.\n");
        return;
    }

    int8 line[256];
    int16 index = 1;

    while (fgets(line, sizeof(line), file))
    {
        Ownership ownership;

        sscanf(line, "%63[^|]|%127[^|]", ownership.username, ownership.db_name);

        if (strcmp(ownership.username, client->username) == 0)
        {
            // Can't convert to (char *) directly
            int8 idx_str[8];
            snprintf((char *)idx_str, sizeof(idx_str), "%d. ", index);
            write_to_client(client, (char *)idx_str);
            write_to_client(client, (char *)ownership.db_name);
            write_to_client(client, "\n");

            index++;
        }
    }

    fclose(file);
}

void load_tree(Client *client)
{
    if (strlen(client->active_db) == 0)
    {
        write_to_client(client, "ERR: No database selected.\n");
        return;
    }

    // If there is an active db, we will load the tree
    init_tree();

    int8 db_path[256];
    snprintf((char *)db_path, sizeof(db_path), "%s/%s.db", DB_FOLDER, client->active_db);

    FILE *file = fopen(db_path, "r");

    if (file == NULL)
    {
        // Handled in use-database
        return;
    }
    int8 line[256];

    while (fgets((char *)line, sizeof(line), file))
    {
        line[strcspn((char *)line, "\n")] = '\0';

        // Root already exists from init_tree(); the file's "/" line is just a marker.
        if (strcmp((char *)line, "/") == 0)
            continue;

        int8 buffer[256];
        int8 *tokens[8];
        zero(buffer, 256);
        strncpy((char *)buffer, (char *)line, strlen((char *)line));
        int32 token_count = split('|', buffer, tokens, 10);

        if (token_count == 3)
        {
            add_leaf(client, tokens[0], tokens[1], tokens[2]);
        }
        else if (token_count == 1)
        {
            add_node(client, line, false);
        }
    }

    write_to_client(client, "OK: Database loaded successfully.\n");
    fclose(file);
}
