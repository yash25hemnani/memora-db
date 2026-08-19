#include "memora.h"

bool s_continuation;

void mainloop(int16 port)
{
    struct sockaddr_in sock;
    int s, client_fd;

    sock.sin_family = AF_INET;
    sock.sin_port = htons(port);
    sock.sin_addr.s_addr = inet_addr(HOST);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    { // -1 is returned for errors
        perror("socket");
        return;
    }

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(s, (struct sockaddr *)&sock, sizeof(sock)) < 0)
    {
        perror("bind");
    }

    if (listen(s, 20) < 0)
    {
        perror("listen");
    }

    while (s_continuation)
    {
        client_fd = accept(s, NULL, NULL);
        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        printf("Connected successfully!\n");
        fflush(stdout);

        // handle client_fd here
        close(client_fd);
    }
}

int main(int argc, char *argv[])
{
    char *s_port;
    int16 port;

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

    while (s_continuation)
    {
        mainloop(port);
    }

    return 0;
}