#include "users.h"
#include "memora.h"

void init(void)
{
    // Flow
    // Start db - If no admin, would prompt for creation of admin, else would continue login

    // Check if users.db exist
    int8 db_path[256];
    snprintf((char *)db_path, sizeof(db_path), "%s/%s.db", DB_FOLDER, USERS_DB);

    // Create folder if not exists
    if (mkdir(DB_FOLDER, 0755) == -1 && errno != EEXIST)
    {
        perror("mkdir");
        return;
    }

    FILE *file = fopen((char *)db_path, "r");

    // If file exists, check for admin
    if (file)
    {
        int8 line[256];
        rewind(file);

        while (fgets(line, sizeof(line), file))
        {
            User user;
            char role_str[16];

            sscanf(line, "%63[^|]|%127[^|]|%15[^\n]", user.username, user.password_hash, role_str);

            user.role = (strcmp(role_str, ROLE_ADMIN_NAME) == 0) ? ROLE_ADMIN : ROLE_USER;

            if (user.role == ROLE_ADMIN)
            {
                // Admin is found
                // Continue with login flow
                return;
            }
        }
    }
    else
    {
        // If file does not exist, we will create one and start create-admin flow
        file = fopen((char *)db_path, "w");
        create_admin(file);
        return;
    }
}

void create_admin(FILE *file)
{
    int8 username[64];
    int8 password[128];

    prompt((int8 *)"Enter admin username: ", username, sizeof(username));
    prompt((int8 *)"Enter admin password: ", password, sizeof(password));

    fprintf(file, "%s|%s|%s", (char *)username, (char *)password, ROLE_ADMIN_NAME);

    fclose(file);

    printf("Admin created successfully!\n");
    fflush(stdout);
    return;
}

void login(Client *client, int8 *username, int8 *password)
{
    // Check if already logged in
    if (client->logged_in == 1) {
        write_to_client(client, "ERR: Already logged in.\n");
        return;
    }

    // Read files
    int8 file_path[256];
    snprintf(file_path, sizeof(file_path), "%s/%s.db", DB_FOLDER, USERS_DB);

    FILE *file = fopen(file_path, "r");

    // We will not create a file here since that should already be handled by init
    if (!file)
    {
        write_to_client(client, "ERR: Users database not found.\n");
        return;
    }

    // If file found, we need to search
    int8 line[256];
    rewind(file);

    while (fgets(line, sizeof(line), file))
    {
        User user;
        char role_str[16];

        sscanf(line, "%63[^|]|%127[^|]|%15[^\n]", user.username, user.password_hash, role_str);

        user.role = (strcmp(role_str, ROLE_ADMIN_NAME) == 0) ? ROLE_ADMIN : ROLE_USER;

        if (strcmp(user.username, username) == 0) {
            fclose(file);
            if (strcmp(user.password_hash, password) == 0) {
                strncpy(client->username, (char *)username, sizeof(client->username) - 1);
                client->username[sizeof(client->username) - 1] = '\0';
                client->logged_in = 1;
                write_to_client(client, "OK: Logged in successfully.\n");
            } else {
                write_to_client(client, "ERR: Wrong password.\n");
            }
            return;
        }
    }

    fclose(file);
    write_to_client(client, "ERR: No such user exists.\n");
    return;
}

void logout(Client *client) {
    client->logged_in = 0;
    write_to_client(client, "OK: Logged out successfully.\n");
    c_continuation = false;
    return;
}

