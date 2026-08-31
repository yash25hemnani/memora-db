#ifndef DATABASE_H
#define DATABASE_H

#include "utils.h"
#include "memora.h"

#define DB_FOLDER "database"

int16 add_ownership(Client *client, int8 *db_name);
int16 create_database(Client *client, int8 *db_name);
int16 delete_database(Client *client, int8 *db_name);
int16 rename_database(Client *client, int8 *old_name, int8 *new_name);
void use_database(Client *client, int8 *db_name);

void list_databases(Client *client);

#endif
