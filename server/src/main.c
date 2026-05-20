#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common/platform.h"

static void usage(const char* prog) {
    fprintf(stderr, "Usage: %s <port>\n", prog);
}

int main(int argc, char** argv) {
    if (argc != 2) { usage(argv[0]); return 1; }
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) { fprintf(stderr, "Bad port\n"); return 1; }

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
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);

    int opt = 1;
#ifdef _WIN32
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "bind failed, err=%d\n", net_last_error());
        sock_close(s);
        net_cleanup();
        return 1;
    }

    if (listen(s, 16) != 0) {
        fprintf(stderr, "listen failed, err=%d\n", net_last_error());
        sock_close(s);
        net_cleanup();
        return 1;
    }

    printf("Server listening on port %d...\n", port);

    struct sockaddr_in caddr;
#ifdef _WIN32
    int clen = sizeof(caddr);
#else
    socklen_t clen = sizeof(caddr);
#endif
    sock_t cs = accept(s, (struct sockaddr*)&caddr, &clen);
    if (cs == SOCK_INVALID) {
        fprintf(stderr, "accept failed, err=%d\n", net_last_error());
    } else {
        printf("Client connected.\n");
        sock_close(cs);
    }

    sock_close(s);
    net_cleanup();
    return 0;
}
