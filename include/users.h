#ifndef USERS_H
#define USERS_H

#include "utils.h"
#include "memora.h"

typedef struct s_user
{
    char username[64];
    char password_hash[128];
    UserRole role;
} User;

typedef struct s_ownership
{
    char username[64];
    char db_name[128];
} Ownership;

void init(void);
void login(Client *client, int8 *username, int8 *password);
void logout(Client *client);
void create_user(Client *client, int8 *username, int8 *password);
void delete_user(Client *client, int8 *username);

#define DB_FOLDER "database"
#define USERS_DB "users"
#define OWNERSHIP_DB "owners"

#endif
