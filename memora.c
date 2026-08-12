#include "memora.h"

bool server_continuation;

void mainloop() {
    struct sockaddr_in sock;
    int s;

    sock.sin_family = AF_INET;
    sock.sin_port = htons(PORT);
    sock.sin_addr.s_addr = inet_addr(HOST);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        return;
    }

    if (bind(s, (struct sockaddr *)&sock, sizeof(sock)) < 0) {
        perror("bind");
    }

    close(s);
}

int main(int argc, char *argv[])
{

    server_continuation = true;

    while (server_continuation)
    {
        mainloop();
    }

    return 0;
}