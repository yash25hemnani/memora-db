#ifndef MEMORA_H
#define MEMORA_H

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <assert.h>
#include <errno.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "utils.h"

#define HOST    "127.0.0.1"
#define PORT    "8008"

#define MEMORA_VERSION  "v1.0.0"
#define MEMORA_AUTHOR   "Yash Hemnani"

typedef enum
{
    ROLE_ADMIN,
    ROLE_USER
} UserRole;

#define ROLE_ADMIN_NAME "admin"
#define ROLE_USER_NAME  "user"

struct s_client {
    int fd;
    char ip[16];
    int16 port;
    char username[64];
    int logged_in;
    char active_db[64];
    UserRole role;
};

typedef struct s_client Client;

enum {
    CMD_OK       = 0,
    CMD_ERR_ARGS = 1,
};

// Client FD, folder name, args array
typedef int32 (*Callback)(Client *, int32 argc, int8 argv[][64]);

struct s_cmdhandler {
    int8 *cmd;
    int8 min_args;   // Excluding cmd
    int8 max_args;   // Excluding cmd
    Callback handler;
    bool requires_login;
};

typedef struct s_cmdhandler CmdHandler;

// Set to false to stop the accept loop in initserver()/child_loop() (defined in memora.c)
extern bool s_continuation;
extern bool c_continuation;

extern CmdHandler handlers[];

int32 ping_handler(Client *client, int32 argc, int8 argv[][64]);
int32 tree_handler(Client *client, int32 argc, int8 argv[][64]);
CmdHandler *get_handler(int8 *command);
int8 verify_arg_counts(Client *client, int8 token_count, int8 max_args, int8 min_args);
void write_to_client(Client *client, char *str);
void child_loop(Client *client);
void mainloop(int server_fd);
void initserver(int16 port);

#endif

