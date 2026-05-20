#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common/platform.h"
#include "../../common/net_frame.h"

static void usage(const char* prog) {
    fprintf(stderr, "Usage: %s <host> <port> <username>\n", prog);
}

int main(int argc, char** argv) {
    if (argc != 4) { usage(argv[0]); return 1; }
    const char* host = argv[1];
    int port = atoi(argv[2]);
    const char* user = argv[3];

    if (port <= 0 || port > 65535) { fprintf(stderr, "Bad port\n"); return 1; }
    if (strlen(user) == 0) { fprintf(stderr, "Bad username\n"); return 1; }

    if (net_init() != 0) {
        fprintf(stderr, "net_init failed, err=%d\n", net_last_error());
        return 1;
    }

    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == SOCK_INVALID) {
        fprintf(stderr, "socket failed, err=%d\n", net_last_error());
        net_cleanup();
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "inet_pton failed (use IPv4 like 127.0.0.1)\n");
        sock_close(s);
        net_cleanup();
        return 1;
    }

    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "connect failed, err=%d\n", net_last_error());
        sock_close(s);
        net_cleanup();
        return 1;
    }

    printf("Connected to %s:%d as %s\n", host, port, user);
const char* msg = "type=ping;text=hello";
if (send_frame(s, msg, (uint32_t)strlen(msg)) != 0) {
    fprintf(stderr, "send_frame failed, err=%d\n", net_last_error());
}
    sock_close(s);
    net_cleanup();
    return 0;
}
