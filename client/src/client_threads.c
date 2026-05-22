#include "client_threads.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef _WIN32
 #include <signal.h>
#endif

#include "../../common/net_frame.h"
#include "../../common/kv.h"
#include "../../common/urlcodec.h"


#ifdef _WIN32
  #include <windows.h>
  typedef HANDLE thread_t;
  static DWORD WINAPI recv_thread_fn(LPVOID arg);
#else
  #include <pthread.h>
  typedef pthread_t thread_t;
  static void* recv_thread_fn(void* arg);
#endif

typedef struct RecvCtx {
 sock_t s;
 volatile int running;
#ifndef _WIN32
 pthread_t main_thread;
#endif
} RecvCtx;

static void trim_newline(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) {
        s[n-1] = '\0';
        n--;
    }
}
static int validate_room_name(const char* room) {
 if (!room || !room[0]) return 0;
 // keep consistent with server MAX_ROOM_NAME (32 incl '\0')
 if (strlen(room) >= 32) return 0;
 return 1;
}

static void print_incoming(const char* payload) {
    char type[32];
    if (!kv_get(payload, "type", type, sizeof(type))) {
         if (strcmp(type, "users") == 0) {
  char list[1600] = {0};
  kv_get(payload, "list", list, sizeof(list));
  printf("[users] %s\n", list[0] ? list : "<empty>");
  return;
 }
  if (strcmp(type, "history_item") == 0) {
 char chat[32] = {0};
 char line_enc[1600] = {0};
 char line[1600] = {0};

 kv_get(payload, "chat", chat, sizeof(chat));
 kv_get(payload, "line", line_enc, sizeof(line_enc));

 if (url_decode(line_enc, line, sizeof(line)) != 0) {
  snprintf(line, sizeof(line), "<bad encoding>");
 }

 printf("[history %s] %s\n", chat[0] ? chat : "?", line);
 return;
}

 if (strcmp(type, "history_end") == 0) {
  printf("[history] end\n");
  return;
 }
        printf("[server] %s\n", payload);
        return;
    }

    if (strcmp(type, "deliver") == 0) {
 char from[64] = {0};
 char text_enc[1024] = {0};
 char text[1024] = {0};

 kv_get(payload, "from", from, sizeof(from));
 kv_get(payload, "text", text_enc, sizeof(text_enc));

 if (url_decode(text_enc, text, sizeof(text)) != 0) {
  snprintf(text, sizeof(text), "<bad encoding>");
 }

 printf("%s: %s\n", from[0] ? from : "?", text);
 return;
}

 if (strcmp(type, "room_deliver") == 0) {
 char room[64] = {0};
 char from[64] = {0};
 char text_enc[1024] = {0};
 char text[1024] = {0};

 kv_get(payload, "room", room, sizeof(room));
 kv_get(payload, "from", from, sizeof(from));
 kv_get(payload, "text", text_enc, sizeof(text_enc));

 if (url_decode(text_enc, text, sizeof(text)) != 0) {
  snprintf(text, sizeof(text), "<bad encoding>");
 }

 printf("[%s] %s: %s\n", room[0] ? room : "room", from[0] ? from : "?", text);
 return;
}
    if (strcmp(type, "info") == 0 || strcmp(type, "error") == 0) {
        char text[1024] = {0};
        kv_get(payload, "text", text, sizeof(text));
         if (strcmp(type, "info") == 0 && strcmp(text, "delivered") == 0) {
  return;
 }
        printf("[%s] %s\n", type, text[0] ? text : payload);
        return;
    }

    printf("[server] %s\n", payload);
}

#ifdef _WIN32
static DWORD WINAPI recv_thread_fn(LPVOID arg)
#else
static void* recv_thread_fn(void* arg)
#endif
{
    RecvCtx* ctx = (RecvCtx*)arg;
    char payload[2048 + 1];

while (ctx->running) {
 uint32_t n = 0;
 int rr = recv_frame(ctx->s, payload, 2048, &n);
 if (rr != 0) {
  printf("[info] connection closed\n");
  fflush(stdout);
  break;
 }
 payload[n] = '\0';
 print_incoming(payload);
 fflush(stdout);
}
ctx->running = 0;

#ifndef _WIN32
// Interrupt blocking fgets in main thread
pthread_kill(ctx->main_thread, SIGINT);
#endif
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static int start_thread(thread_t* t, RecvCtx* ctx) {
#ifdef _WIN32
    *t = CreateThread(NULL, 0, recv_thread_fn, ctx, 0, NULL);
    return *t ? 0 : -1;
#else
    return pthread_create(t, NULL, recv_thread_fn, ctx);
#endif
}

static void join_thread(thread_t t) {
#ifdef _WIN32
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
#else
    pthread_join(t, NULL);
#endif
}

int run_client(sock_t s, const char* username) {
    // Send login
    char login_msg[256];
    snprintf(login_msg, sizeof(login_msg), "type=login;user=%s", username);
    if (send_frame(s, login_msg, (uint32_t)strlen(login_msg)) != 0) {
        fprintf(stderr, "send_frame(login) failed\n");
        return 1;
    }

    // Start receiver thread
    RecvCtx ctx;
    ctx.s = s;
    ctx.running = 1;
    #ifndef _WIN32
ctx.main_thread = pthread_self();
#endif

    thread_t thr;
    if (start_thread(&thr, &ctx) != 0) {
        fprintf(stderr, "cannot start recv thread\n");
        return 1;
    }

    printf("Commands:\n");
 printf(" /msg <user> <text>\n");
 printf(" /create <room>\n");
 printf(" /join <room>\n");
 printf(" /leave <room>\n");
 printf(" /room <room> <text>\n");
 printf(" /users\n");
 printf(" /history <user>\n");
printf(" /history_room <room>\n");
 printf(" /quit\n");

    char line[1400];
    while (ctx.running) {
        printf("> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;
        trim_newline(line);

        if (strcmp(line, "/quit") == 0) {
            break;
        }

 if (strncmp(line, "/msg ", 5) == 0) {
  char* p = line + 5;
  while (*p == ' ') p++;
  char* to = p;
  char* sp = strchr(to, ' ');
  if (!sp) {
   printf("[error] usage: /msg <user> <text>\n");
   continue;
  }
  *sp = '\0';
  char* text = sp + 1;
  while (*text == ' ') text++;

  if (!to[0] || !text[0]) {
   printf("[error] usage: /msg <user> <text>\n");
   continue;
  }

  char text_enc[1400];
  if (url_encode(text, text_enc, sizeof(text_enc)) != 0) {
   printf("[error] message too long\n");
   continue;
  }

  char out[2048];
  snprintf(out, sizeof(out), "type=msg;to=%s;text=%s", to, text_enc);
  if (send_frame(s, out, (uint32_t)strlen(out)) != 0) {
   printf("[error] send failed\n");
   ctx.running = 0;
   break;
  }
  continue;
 }
 if (strncmp(line, "/create ", 8) == 0) {
  char* room = line + 8;
  while (*room == ' ') room++;
  if (!validate_room_name(room)) { printf("[error] bad room name (1..31 chars)\n"); continue; }

  char out[256];
  snprintf(out, sizeof(out), "type=room_create;room=%s", room);

  if (send_frame(s, out, (uint32_t)strlen(out)) != 0) {
   printf("[error] send failed\n");
   ctx.running = 0;
   break;
  }
  continue;
 }

 if (strncmp(line, "/join ", 6) == 0) {
  char* room = line + 6;
  while (*room == ' ') room++;
  if (!validate_room_name(room)) { printf("[error] bad room name (1..31 chars)\n"); continue; }
  char out[512];
  snprintf(out, sizeof(out), "type=room_join;room=%s", room);
  if (send_frame(s, out, (uint32_t)strlen(out)) != 0) {
   printf("[error] send failed\n");
   ctx.running = 0;
   break;
  }
  continue;
 }

 if (strncmp(line, "/leave ", 7) == 0) {
  char* room = line + 7;
  while (*room == ' ') room++;
  if (!validate_room_name(room)) { printf("[error] bad room name (1..31 chars)\n"); continue; }
  char out[512];
  snprintf(out, sizeof(out), "type=room_leave;room=%s", room);
  if (send_frame(s, out, (uint32_t)strlen(out)) != 0) {
   printf("[error] send failed\n");
   ctx.running = 0;
   break;
  }
  continue;
 }

 if (strncmp(line, "/room ", 6) == 0) {
  char* p = line + 6;
  while (*p == ' ') p++;
  char* room = p;
  char* sp = strchr(room, ' ');
  if (!sp) { printf("[error] usage: /room <room> <text>\n"); continue; }
  *sp = '\0';
  char* text = sp + 1;
  while (*text == ' ') text++;
  if (!room[0] || !text[0]) { printf("[error] usage: /room <room> <text>\n"); continue; }

  char text_enc[1400];
  if (url_encode(text, text_enc, sizeof(text_enc)) != 0) {
   printf("[error] message too long\n");
   continue;
  }

  char out[2048];
  snprintf(out, sizeof(out), "type=room_msg;room=%s;text=%s", room, text_enc);
  if (send_frame(s, out, (uint32_t)strlen(out)) != 0) {
   printf("[error] send failed\n");
   ctx.running = 0;
   break;
  }
  continue;
 }
  if (strcmp(line, "/users") == 0) {
  const char* out = "type=users";
  if (send_frame(s, out, (uint32_t)strlen(out)) != 0) {
   printf("[error] send failed\n");
   ctx.running = 0;
   break;
  }
  continue;
 }
  if (strncmp(line, "/history_room ", 14) == 0) {
  char* room = line + 14;
  while (*room == ' ') room++;
  if (!room[0]) { printf("[error] usage: /history_room <room>\n"); continue; }
  char out[512];
  snprintf(out, sizeof(out), "type=history_room;room=%s;limit=20", room);
  if (send_frame(s, out, (uint32_t)strlen(out)) != 0) {
   printf("[error] send failed\n");
   ctx.running = 0;
   break;
  }
  continue;
 }

 if (strncmp(line, "/history ", 9) == 0) {
  char* peer = line + 9;
  while (*peer == ' ') peer++;
  if (!peer[0]) { printf("[error] usage: /history <user>\n"); continue; }
  char out[512];
  snprintf(out, sizeof(out), "type=history_dm;peer=%s;limit=20", peer);
  if (send_frame(s, out, (uint32_t)strlen(out)) != 0) {
   printf("[error] send failed\n");
   ctx.running = 0;
   break;
  }
  continue;
 }
        printf("[error] unknown command\n");
    }

    ctx.running = 0;
    // closing socket will also unblock recv thread
    sock_close(s);
    join_thread(thr);
    return 0;
}
