#include "memora.h"

// Set to false to stop the accept loop in initserver()
bool s_continuation;
bool c_continuation;

void zero(int8 *buffer, int16 size)
{
    int8 *p;
    int16 n;

    for (n = 0, p = buffer; n < size; n++, p++)
    {
        *p = 0;
    }

    return;
}

#define MAX_TOKENS 10


int split(int8 *buffer, int8 *tokens[], int8 max_tokens)
{
    int8 token_count = 0;
    int8 *p = buffer;

    // Continue loop till end of line or where it exceeds the number of tokens allowed
    while (*p != '\0' && token_count < max_tokens)
    {
        // Another while loop that moves through the buffer
        // First we skip all empty spaces
        while (*p == ' ' || *p == '\n')
        {
            p++;
        }

        if (*p == '\0')
        {
            break;
        }

        tokens[token_count] = p;
        token_count++;

        // Find end of token
        while (*p != '\0' && *p != ' ' && *p != '\n')
        {
            p++;
        }

        // Terminate token
        if (*p != '\0')
        {
            *p = '\0';
            p++;
        }
    }

    return token_count;
}

void child_loop(Client *client)
{
    int8 buffer[256];
    int8 *tokens[MAX_TOKENS];
    int8 token_count;

    zero(buffer, 256);
    read(client->fd, buffer, 255);

    token_count = split(buffer, tokens, MAX_TOKENS);

    for (int8 i = 0; i < token_count; i++)
    {
        printf("[%d] %s\n", i, (char *)tokens[i]);
    }
    fflush(stdout);

    char *ack = "OK\n";
    write(client->fd, ack, strlen(ack));

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
        printf("Status:100 - Connected to Memora\n");
        fflush(stdout);
        while (c_continuation)
        {
            child_loop(client);
        }

        free(client);
        close(server_fd);
        return;
    }

    close(client_fd);
    free(client);
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

    printf("Server listening on port %d\n", port);
    fflush(stdout);

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
