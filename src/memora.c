#include "memora.h"
#include "tree.h"
#include "users.h"
#include "database.h"
#include "hash_table.h"

// Set to false to stop the accept loop in initserver()
bool s_continuation;
bool c_continuation;

static const char *BANNER =
"███╗   ███╗███████╗███╗   ███╗ ██████╗ ██████╗  █████╗ \n"
"████╗ ████║██╔════╝████╗ ████║██╔═══██╗██╔══██╗██╔══██╗\n"
"██╔████╔██║█████╗  ██╔████╔██║██║   ██║██████╔╝███████║\n"
"██║╚██╔╝██║██╔══╝  ██║╚██╔╝██║██║   ██║██╔══██╗██╔══██║\n"
"██║ ╚═╝ ██║███████╗██║ ╚═╝ ██║╚██████╔╝██║  ██║██║  ██║\n"
"╚═╝     ╚═╝╚══════╝╚═╝     ╚═╝ ╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═╝\n"
"---------------------------------------------------------\n"
"MEMORA " MEMORA_VERSION "\n"
"Created by " MEMORA_AUTHOR "\n"
"---------------------------------------------------------\n";

// Writes a NUL-terminated string to the client socket; no-op on an empty string.
void write_to_client(Client *client, char *str)
{
    int16 size;

    size = (int16)strlen(str);
    if (size)
    {
        write(client->fd, str, size);
    }
}

int32 ping_handler(Client *client, int32 argc, int8 argv[][64])
{
    // This command should not have any arguments
    if (argc != 0)
    {
        write_to_client(client, "ERR: Too many arguments.\n");
        return CMD_OK;
    }

    write_to_client(client, "pong\n");
    return CMD_ERR_ARGS;
}

int32 tree_handler(Client *client, int32 argc, int8 argv[][64])
{
    // This command should not have any arguments
    if (argc != 0)
    {
        write_to_client(client, "ERR: Too many arguments.\n");
        return CMD_ERR_ARGS;
    }

    print_tree(client, root);
    return CMD_OK;
}

void search_node_handler(Client *client, int32 argc, int8 argv[][64])
{
    int8 *path = argv[0];

    Node *node = find_node(client, &root->node, path);

    if (node)
    {
        write_to_client(client, "OK: Node found.\n");
    }
    else
    {
        write_to_client(client, "ERR: Node not found.\n");
    }
}

void create_node_handler(Client *client, int32 argc, int8 argv[][64])
{
    // Accepets two args - path and folder
    int8 *path = argv[0];

    add_node(client, path, true);
}

void remove_node_handler(Client *client, int32 argc, int8 argv[][64])
{
    // Accepets two args - path and folder
    int8 *path = argv[0];

    remove_node(client, path);
}

void login_handler(Client *client, int32 argc, int8 argv[][64])
{
    // Accepets two args - path and folder
    int8 *username = argv[0];
    int8 *password = argv[1];

    login(client, username, password);
}

void logout_handler(Client *client, int32 argc, int8 argv[][64])
{
    logout(client);
}

void create_user_handler(Client *client, int32 argc, int8 argv[][64])
{
    int8 *username = argv[0];
    int8 *password = argv[1];

    create_user(client, username, password);
}

void delete_user_handler(Client *client, int32 argc, int8 argv[][64])
{
    int8 *username = argv[0];

    delete_user(client, username);
}

void create_database_handler(Client *client, int32 argc, int8 argv[][64])
{
    int8 *db_name = argv[0];

    create_database(client, db_name);
}

void use_database_handler(Client *client, int32 argc, int8 argv[][64])
{
    int8 *db_name = argv[0];

    use_database(client, db_name);
}

void delete_database_handler(Client *client, int32 argc, int8 argv[][64])
{
    int8 *db_name = argv[0];

    delete_database(client, db_name);
}

void rename_database_handler(Client *client, int32 argc, int8 argv[][64])
{
    int8 *old_name = argv[0];
    int8 *new_name = argv[1];

    rename_database(client, old_name, new_name);
}

void list_database_handler(Client *client, int32 argc, int8 argv[][64])
{
    int8 *db_name = argv[0];

    list_databases(client);
}
void print_hash_table_handler(Client *client, int32 argc, int8 argv[][64])
{
    int8 *db_name = argv[0];

    print_hash_table(client, hash_table);
}

void create_leaf_handler(Client *client, int32 argc, int8 argv[][64])
{
    int8 *path = argv[0];
    int8 *key = argv[1];
    int8 *value = argv[2];

    add_leaf(client, path, key, value);
}

void delete_leaf_handler(Client *client, int32 argc, int8 argv[][64])
{
    int8 *path = argv[0];
    int8 *key = argv[1];

    remove_leaf(client, path, key);
}

void print_leaves_handler(Client *client, int32 argc, int8 argv[][64])
{
    int8 *path = argv[0];

    print_leaves(client, path);
}

void update_leaf_handler(Client *client, int32 argc, int8 argv[][64])
{
    int8 *path = argv[0];
    int8 *key = argv[1];
    int8 *value = argv[2];

    update_leaf(client, path, key, value);
}

CmdHandler handlers[] = {
    {(int8 *)"ping", 0, 0, ping_handler, false},
    {(int8 *)"tree", 0, 0, tree_handler, true},
    {(int8 *)"login", 2, 2, login_handler},
    {(int8 *)"logout", 0, 0, logout_handler, true},
    {(int8 *)"create-user", 2, 2, create_user_handler, true},
    {(int8 *)"delete-user", 1, 1, delete_user_handler, true},
    {(int8 *)"search-node", 1, 1, search_node_handler, true},
    {(int8 *)"create-node", 1, 1, create_node_handler, true},
    {(int8 *)"remove-node", 1, 1, remove_node_handler, true},
    {(int8 *)"create-database", 1, 1, create_database_handler, true},
    {(int8 *)"use-database", 1, 1, use_database_handler, true},
    {(int8 *)"delete-database", 1, 1, delete_database_handler, true},
    {(int8 *)"rename-database", 2, 2, rename_database_handler, true},
    {(int8 *)"list-databases", 0, 0, list_database_handler, true},
    {(int8 *)"hash-table", 0, 0, print_hash_table_handler, true},
    {(int8 *)"create-leaf", 3, 3, create_leaf_handler, true},
    {(int8 *)"delete-leaf", 2, 2, delete_leaf_handler, true},
    {(int8 *)"update-leaf", 3, 3, update_leaf_handler, true},
    {(int8 *)"print-leaves", 1, 1, print_leaves_handler, true},
};

// Get pointer to that function
CmdHandler *get_handler(int8 *command)
{
    int count = sizeof(handlers) / sizeof(handlers[0]);

    for (int8 i = 0; i < count; i++)
    {
        if (strcmp((char *)handlers[i].cmd, (char *)command) == 0)
        {
            return &handlers[i];
        }
    }

    return NULL;
}

int8 verify_arg_counts(Client *client, int8 token_count, int8 max_args, int8 min_args)
{
    if (token_count > max_args)
    {
        write_to_client(client, "ERR: Too many arguments.\n");
        return STATUS_FALSE;
    }

    if (token_count < min_args)
    {
        write_to_client(client, "ERR: Too few arguments.\n");
        return STATUS_FALSE;
    }

    return STATUS_OK;
}

#define MAX_TOKENS 10
#define MAX_ARGS (MAX_TOKENS - 2)

void child_loop(Client *client)
{
    int8 buffer[256];
    int8 *tokens[MAX_TOKENS];
    int8 token_count;
    int8 cmd[16];
    int8 args[MAX_ARGS][64];
    int n;

    zero(buffer, 256);
    n = read(client->fd, buffer, 255);

    if (n <= 0)
    {
        // Client disconnected (n == 0) or read failed (n < 0); stop this connection.
        c_continuation = false;
        return;
    }

    token_count = split(' ', buffer, tokens, MAX_TOKENS);

    for (int8 i = 0; i < token_count; i++)
    {
        printf("[%d] %s\n", i, (char *)tokens[i]);
    }
    fflush(stdout);

    // Check if token count is zero
    if (token_count < 1)
    {
        write_to_client(client, "ERR: Empty command.\n");
        return;
    }

    // Store the command
    zero(cmd, 16);
    strncpy((char *)cmd, (char *)tokens[0], 15);

    CmdHandler *handler = get_handler(cmd);

    if (handler == NULL)
    {
        write_to_client(client, "ERR: Unknown command.\n");
        return;
    }

    if (handler->requires_login && !client->logged_in)
    {
        write_to_client(client, "ERR: Login required.\n");
        return;
    }

    // Verify arg counts
    int8 argc = token_count - 1;
    int8 verify = verify_arg_counts(client, argc, handler->max_args, handler->min_args);

    if (!verify)
        return;

    // Arguments
    for (int8 i = 0; i < argc; i++)
    {
        zero(args[i], 64);
        strncpy((char *)args[i], (char *)tokens[i + 1], 63);
    }

    handler->handler(client, argc, args);

    return;
}

// Accepts one client connection on server_fd, logs it, and closes it.
void mainloop(int server_fd)
{
    struct sockaddr_in cli;
    // accept() requires this to be pre-set to the size of cli.
    socklen_t len = sizeof(cli);
    int client_fd;
    char *ip;
    int16 port;
    Client *client;
    pid_t pid;

    client_fd = accept(server_fd, (struct sockaddr *)&cli, &len);

    if (client_fd < 0)
    {
        // No connection ready or accept() failed; avoid busy-looping.
        sleep(1);
        return;
    }

    port = (int16)ntohs(cli.sin_port);
    ip = inet_ntoa(cli.sin_addr);

    printf("Connection from %s:%d\n", ip, port);
    fflush(stdout);

    client = (Client *)malloc(sizeof(struct s_client));
    assert(client);

    zero((int8 *)client, sizeof(struct s_client));
    client->fd = client_fd;
    client->port = port;
    strncpy(client->ip, ip, 15);

    pid = fork();

    if (pid)
    {
        // Parent Process - Free to accept more connections
        free(client);
        return;
    }
    else
    {
        // Child Process - Handle that particular client
        c_continuation = true;
        write_to_client(client, (char *)BANNER);
        write_to_client(client, "OK: Connected to Memora.\n");

        while (c_continuation)
        {
            child_loop(client);
        }

        close(client_fd);
        free(client);
        exit(0);
    }
}

// Creates a listening socket on the given port and serves connections
// until s_continuation is set to false.
void initserver(int16 port)
{
    struct sockaddr_in sock; // IPv4 address/port to bind to
    int server_fd;           // fd for the listening socket

    sock.sin_family = AF_INET;              // use IPv4
    sock.sin_port = htons(port);            // port, in network byte order
    sock.sin_addr.s_addr = inet_addr(HOST); // bind address

    server_fd = socket(AF_INET, SOCK_STREAM, 0); // create a TCP socket
    if (server_fd < 0)
    {
        perror("socket");
        return;
    }

    // Allow immediate rebinding to the port after restart.
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd, (struct sockaddr *)&sock, sizeof(sock)) < 0)
    {
        perror("bind");
    }

    if (listen(server_fd, 20) < 0)
    {
        perror("listen");
    }

    printf(BANNER);
    printf("Server listening on port %d\n", port);
    fflush(stdout);

    init();

    while (s_continuation)
    {
        mainloop(server_fd);
    }
}

int main(int argc, char *argv[])
{
    char *s_port;
    int16 port;

    // Use the port given on the command line, or the default.
    if (argc < 2)
    {
        s_port = PORT;
    }
    else
    {
        s_port = argv[1];
    }

    port = (int16)atoi(s_port);

    s_continuation = true;

    initserver(port);

    return 0;
}
