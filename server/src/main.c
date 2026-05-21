#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common/platform.h"
#include "../../common/net_frame.h"
#include "../../common/kv.h"

#define MAX_CLIENTS 64
#define MAX_USER    32
#define MAX_PAYLOAD 2048

typedef struct Client {
    sock_t sock;
    int   used;
    char  user[MAX_USER];
} Client;

static void usage(const char* prog) {
    fprintf(stderr, "Usage: %s <port>\n", prog);
}

static int add_client(Client* clients, sock_t s) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].used) {
            clients[i].used = 1;
            clients[i].sock = s;
            clients[i].user[0] = '\0';
            return i;
        }
    }
    return -1;
}

static void remove_client(Client* clients, int idx) {
    if (idx < 0 || idx >= MAX_CLIENTS) return;
    if (clients[idx].used) {
        sock_close(clients[idx].sock);
        clients[idx].used = 0;
        clients[idx].sock = SOCK_INVALID;
        clients[idx].user[0] = '\0';
    }
}

static int handle_frame(Client* c, const char* payload) {
    char type[32];
    if (!kv_get(payload, "type", type, sizeof(type))) {
        return 0;
    }

    if (strcmp(type, "login") == 0) {
        char user[MAX_USER];
        if (!kv_get(payload, "user", user, sizeof(user))) {
            const char* err = "type=error;text=missing user";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }
        strncpy(c->user, user, sizeof(c->user) - 1);
        c->user[sizeof(c->user) - 1] = '\0';

        char info[128];
        snprintf(info, sizeof(info), "type=info;text=login ok (%s)", c->user);
        send_frame(c->sock, info, (uint32_t)strlen(info));

        printf("User logged in: %s\n", c->user);
        return 0;
    }

    // for now: unknown types
    const char* info = "type=info;text=unknown command";
    send_frame(c->sock, info, (uint32_t)strlen(info));
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 2) { usage(argv[0]); return 1; }
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) { fprintf(stderr, "Bad port\n"); return 1; }

    if (net_init() != 0) {
        fprintf(stderr, "net_init failed, err=%d\n", net_last_error());
        return 1;
    }

    sock_t ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls == SOCK_INVALID) {
        fprintf(stderr, "socket failed, err=%d\n", net_last_error());
        net_cleanup();
        return 1;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);

    if (bind(ls, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "bind failed, err=%d\n", net_last_error());
        sock_close(ls);
        net_cleanup();
        return 1;
    }

    if (listen(ls, 16) != 0) {
        fprintf(stderr, "listen failed, err=%d\n", net_last_error());
        sock_close(ls);
        net_cleanup();
        return 1;
    }

    printf("Server listening on port %d...\n", port);

    Client clients[MAX_CLIENTS];
    memset(clients, 0, sizeof(clients));
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].sock = SOCK_INVALID;

    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(ls, &rfds);

        sock_t maxfd = ls;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].used) {
                FD_SET(clients[i].sock, &rfds);
                if (clients[i].sock > maxfd) maxfd = clients[i].sock;
            }
        }

        int rc = select((int)(maxfd + 1), &rfds, NULL, NULL, NULL);
        if (rc < 0) {
            fprintf(stderr, "select failed, err=%d\n", net_last_error());
            continue;
        }

        if (FD_ISSET(ls, &rfds)) {
            struct sockaddr_in caddr;
#ifdef _WIN32
            int clen = sizeof(caddr);
#else
            socklen_t clen = sizeof(caddr);
#endif
            sock_t cs = accept(ls, (struct sockaddr*)&caddr, &clen);
            if (cs != SOCK_INVALID) {
                int idx = add_client(clients, cs);
                if (idx < 0) {
                    const char* err = "type=error;text=server full";
                    send_frame(cs, err, (uint32_t)strlen(err));
                    sock_close(cs);
                } else {
                    printf("Client connected (slot=%d).\n", idx);
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].used) continue;
            if (!FD_ISSET(clients[i].sock, &rfds)) continue;

            char payload[MAX_PAYLOAD + 1];
            uint32_t n = 0;
            int rr = recv_frame(clients[i].sock, payload, (uint32_t)MAX_PAYLOAD, &n);
            if (rr != 0) {
                printf("Client disconnected (slot=%d, user=%s)\n", i,
                       clients[i].user[0] ? clients[i].user : "<none>");
                remove_client(clients, i);
                continue;
            }

            payload[n] = '\0';
            handle_frame(&clients[i], payload);
        }
    }

    // unreachable for now
    sock_close(ls);
    net_cleanup();
    return 0;
}

