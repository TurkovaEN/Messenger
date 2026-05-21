#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../common/net_frame.h"
#include "../../common/kv.h"
#include "logger.h"
#include <signal.h>

#define MAX_CLIENTS 64
#define MAX_USER    32
#define MAX_PAYLOAD 2048

#define MAX_ROOMS 32
#define MAX_ROOM_NAME 32
#define MAX_ROOM_MEMBERS 64

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

typedef struct Client {
    sock_t sock;
    int   used;
    char  user[MAX_USER];
} Client;

typedef struct Room {
 int used;
 char name[MAX_ROOM_NAME];
 int members[MAX_ROOM_MEMBERS]; // indexes in clients[]
 int member_count;
} Room;

static void usage(const char* prog) {
    fprintf(stderr, "Usage: %s <port> <log_path>\n", prog);
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

static int find_by_user(Client* clients, const char* user) {
    if (!user || !user[0]) return -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].used && clients[i].user[0] && strcmp(clients[i].user, user) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_room(Room* rooms, const char* name) {
 if (!name || !name[0]) return -1;
 for (int i = 0; i < MAX_ROOMS; i++) {
  if (rooms[i].used && strcmp(rooms[i].name, name) == 0) return i;
 }
 return -1;
}

static int add_room(Room* rooms, const char* name) {
 if (!name || !name[0]) return -1;
 if (find_room(rooms, name) >= 0) return -2; // already exists
 for (int i = 0; i < MAX_ROOMS; i++) {
  if (!rooms[i].used) {
   rooms[i].used = 1;
   strncpy(rooms[i].name, name, sizeof(rooms[i].name) - 1);
   rooms[i].name[sizeof(rooms[i].name) - 1] = '\0';
   rooms[i].member_count = 0;
   return i;
  }
 }
 return -1; // no slots
}

static int room_has_member(const Room* r, int client_idx) {
 for (int i = 0; i < r->member_count; i++) {
  if (r->members[i] == client_idx) return 1;
 }
 return 0;
}

static int room_add_member(Room* r, int client_idx) {
 if (r->member_count >= MAX_ROOM_MEMBERS) return -1;
 if (room_has_member(r, client_idx)) return 0;
 r->members[r->member_count++] = client_idx;
 return 0;
}

static void room_remove_member(Room* r, int client_idx) {
 for (int i = 0; i < r->member_count; i++) {
  if (r->members[i] == client_idx) {
   r->members[i] = r->members[r->member_count - 1];
   r->member_count--;
   return;
  }
 }
}

static void room_broadcast(Room* r, Client* clients, const char* msg, int skip_idx) {
 for (int i = 0; i < r->member_count; i++) {
  int idx = r->members[i];
  if (idx == skip_idx) continue;
  if (!clients[idx].used) continue;
  if (send_frame(clients[idx].sock, msg, (uint32_t)strlen(msg)) != 0) {
   // ignore errors here; disconnects will be handled by main loop
  }
 }
}

static int handle_frame(Client* clients, Room* rooms, int client_idx, Client* c, const char* payload) {
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
log_info("Login user=%s", c->user);
        return 0;
    }
if (strcmp(type, "msg") == 0) {
    if (!c->user[0]) {
        const char* err = "type=error;text=not logged in";
        send_frame(c->sock, err, (uint32_t)strlen(err));
        return 0;
    }

    char to[MAX_USER];
    char text[1024];

    if (!kv_get(payload, "to", to, sizeof(to))) {
        const char* err = "type=error;text=missing to";
        send_frame(c->sock, err, (uint32_t)strlen(err));
        return 0;
    }
    if (!kv_get(payload, "text", text, sizeof(text))) {
        const char* err = "type=error;text=missing text";
        send_frame(c->sock, err, (uint32_t)strlen(err));
        return 0;
    }

    int dst = find_by_user(clients, to);
    if (dst < 0) {
        const char* err = "type=error;text=user offline";
        send_frame(c->sock, err, (uint32_t)strlen(err));
log_info("MSG from=%s to=%s failed: offline", c->user, to);
        return 0;
    }

    char out[1400];
    snprintf(out, sizeof(out), "type=deliver;from=%s;text=%s", c->user, text);
    if (send_frame(clients[dst].sock, out, (uint32_t)strlen(out)) != 0) {
        const char* err = "type=error;text=deliver failed";
        send_frame(c->sock, err, (uint32_t)strlen(err));
        return 0;
    }

    const char* ok = "type=info;text=delivered";
    send_frame(c->sock, ok, (uint32_t)strlen(ok));
    printf("MSG %s -> %s: %s\n", c->user, to, text);
log_info("MSG from=%s to=%s text=%s", c->user, to, text);
    return 0;
}
 if (strcmp(type, "room_create") == 0) {
  if (!c->user[0]) {
   const char* err = "type=error;text=not logged in";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }
  char room[MAX_ROOM_NAME];
  if (!kv_get(payload, "room", room, sizeof(room))) {
   const char* err = "type=error;text=missing room";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }
  int ar = add_room(rooms, room);
  if (ar == -2) {
   const char* err = "type=error;text=room already exists";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }
  if (ar < 0) {
   const char* err = "type=error;text=cannot create room";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }
  char info[128];
  snprintf(info, sizeof(info), "type=info;text=room created (%s)", room);
  send_frame(c->sock, info, (uint32_t)strlen(info));
  log_info("ROOM create user=%s room=%s", c->user, room);
  return 0;
 }

 if (strcmp(type, "room_join") == 0) {
  if (!c->user[0]) {
   const char* err = "type=error;text=not logged in";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }
  char room[MAX_ROOM_NAME];
  if (!kv_get(payload, "room", room, sizeof(room))) {
   const char* err = "type=error;text=missing room";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }
  int ri = find_room(rooms, room);
  if (ri < 0) {
   const char* err = "type=error;text=no such room";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }
  if (room_add_member(&rooms[ri], client_idx) != 0) {
   const char* err = "type=error;text=room is full";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }
  char info[128];
  snprintf(info, sizeof(info), "type=info;text=joined room (%s)", room);
  send_frame(c->sock, info, (uint32_t)strlen(info));
  log_info("ROOM join user=%s room=%s", c->user, room);
  return 0;
 }

 if (strcmp(type, "room_leave") == 0) {
  if (!c->user[0]) {
   const char* err = "type=error;text=not logged in";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }
  char room[MAX_ROOM_NAME];
  if (!kv_get(payload, "room", room, sizeof(room))) {
   const char* err = "type=error;text=missing room";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }
  int ri = find_room(rooms, room);
  if (ri < 0) {
   const char* err = "type=error;text=no such room";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }
  room_remove_member(&rooms[ri], client_idx);
  char info[128];
  snprintf(info, sizeof(info), "type=info;text=left room (%s)", room);
  send_frame(c->sock, info, (uint32_t)strlen(info));
  log_info("ROOM leave user=%s room=%s", c->user, room);
  return 0;
 }

 if (strcmp(type, "room_msg") == 0) {
  if (!c->user[0]) {
   const char* err = "type=error;text=not logged in";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }
  char room[MAX_ROOM_NAME];
  char text[1024];
  if (!kv_get(payload, "room", room, sizeof(room))) {
   const char* err = "type=error;text=missing room";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }
  if (!kv_get(payload, "text", text, sizeof(text))) {
   const char* err = "type=error;text=missing text";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }
  int ri = find_room(rooms, room);
  if (ri < 0) {
   const char* err = "type=error;text=no such room";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }
  if (!room_has_member(&rooms[ri], client_idx)) {
   const char* err = "type=error;text=not in room";
   send_frame(c->sock, err, (uint32_t)strlen(err));
   return 0;
  }

  char out[MAX_PAYLOAD];
  snprintf(out, sizeof(out), "type=room_deliver;room=%s;from=%s;text=%s", room, c->user, text);
  room_broadcast(&rooms[ri], clients, out, -1);

  const char* ok = "type=info;text=room message sent";
  send_frame(c->sock, ok, (uint32_t)strlen(ok));
  log_info("ROOM msg from=%s room=%s text=%s", c->user, room, text);
  return 0;
 }

    // for now: unknown types
    const char* info = "type=info;text=unknown command";
    send_frame(c->sock, info, (uint32_t)strlen(info));
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 3) { usage(argv[0]); return 1; }
    int port = atoi(argv[1]);
const char* log_path = argv[2];
    if (port <= 0 || port > 65535) { fprintf(stderr, "Bad port\n"); return 1; }

    if (net_init() != 0) {
        fprintf(stderr, "net_init failed, err=%d\n", net_last_error());
        return 1;
    }
if (log_init(log_path) != 0) {
    fprintf(stderr, "Cannot open log file: %s\n", log_path);
    net_cleanup();
    return 1;
}
log_info("Server start (port=%d)", port);

signal(SIGINT, on_signal);
signal(SIGTERM, on_signal);

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

    Room rooms[MAX_ROOMS];
memset(rooms, 0, sizeof(rooms));

while (!g_stop) {
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
#ifndef _WIN32
        if (net_last_error() == EINTR) continue;
#endif
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
                log_info("Client connected (slot=%d)", idx);
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
            log_info("Client disconnected (slot=%d, user=%s)", i,
                     clients[i].user[0] ? clients[i].user : "<none>");
            for (int r = 0; r < MAX_ROOMS; r++) {
 if (rooms[r].used) room_remove_member(&rooms[r], i);
}
            remove_client(clients, i);
            continue;
        }

        payload[n] = '\0';
        handle_frame(clients, rooms, i, &clients[i], payload);
    }
}
for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].used) remove_client(clients, i);
}
sock_close(ls);

log_info("Server stop");
log_close();
net_cleanup();
return 0;
}
