/* Check the browser's NO_SSL socket macros against a local TCP listener. */
#include "xp_sock.h"
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "Failed at line %d: %s (WSA %d)\n", \
            __LINE__, #test, WSAGetLastError()); return 1; \
} } while (0)

int main(void)
{
    WSADATA winsock;
    NETSOCK listener, client, server;
    struct sockaddr_in address;
    int length = sizeof(address);
    int timeout = 3000;
    int received = 0, amount;
    u_long nonblocking = 1;
    char buffer[4];
    CHECK(WSAStartup(MAKEWORD(1, 1), &winsock) == 0);
    listener = NETSOCKET(AF_INET, SOCK_STREAM, 0);
    CHECK(listener != SOCKET_INVALID);
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(NETBIND(listener, (struct sockaddr *)&address, sizeof(address)) == 0);
    CHECK(NETLISTEN(listener, 1) == 0);
    CHECK(NETGETSOCKNAME(listener, (struct sockaddr *)&address, &length) == 0);
    client = NETSOCKET(AF_INET, SOCK_STREAM, 0);
    CHECK(client != SOCKET_INVALID);
    CHECK(NETCONNECT(client, (struct sockaddr *)&address, length) == 0);
    server = NETACCEPT(listener, NULL, NULL);
    CHECK(server != SOCKET_INVALID);
    CHECK(setsockopt(server, SOL_SOCKET, SO_RCVTIMEO,
                     (const char *)&timeout, sizeof(timeout)) == 0);
    CHECK(NETWRITE(client, "test", 4) == 4);
    while (received < sizeof(buffer)) {
        amount = NETREAD(server, buffer + received, sizeof(buffer) - received);
        CHECK(amount > 0);
        received += amount;
    }
    CHECK(memcmp(buffer, "test", 4) == 0);
    CHECK(SOCKET_IOCTL(client, FIONBIO, &nonblocking) == 0);
    CHECK(NETREAD(client, buffer, sizeof(buffer)) == SOCKET_ERROR);
    CHECK(SOCKET_ERRNO == WSAEWOULDBLOCK);
    CHECK(NETCLOSE(server) == 0);
    CHECK(NETCLOSE(client) == 0);
    CHECK(NETCLOSE(listener) == 0);
    CHECK(WSACleanup() == 0);
    puts("Win32 NO_SSL socket loopback passed");
    return 0;
}
