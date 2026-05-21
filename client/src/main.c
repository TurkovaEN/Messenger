#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common/platform.h"
#include "../../common/net_frame.h"
#include "client_threads.h"

static void usage(const char* prog) {
    fprintf(stderr, "Usage:\n");
fprintf(stderr, "  %s <host> <port> <username>\n", prog);
fprintf(stderr, "  %s --register <host> <port> <username>\n", prog);
}

int main(int argc, char** argv) {
 int do_register = 0;
 const char* host = NULL;
 int port = 0;
 const char* user = NULL;

 if (argc == 4) {
  host = argv[1];
  port = atoi(argv[2]);
  user = argv[3];
 } else if (argc == 5 && strcmp(argv[1], "--register") == 0) {
  do_register = 1;
  host = argv[2];
  port = atoi(argv[3]);
  user = argv[4];
 } else {
  usage(argv[0]);
  return 1;
 }
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
    if (do_register) {
  char reg_msg[256];
  snprintf(reg_msg, sizeof(reg_msg), "type=register;user=%s", user);
  if (send_frame(s, reg_msg, (uint32_t)strlen(reg_msg)) != 0) {
   fprintf(stderr, "send_frame(register) failed\n");
   sock_close(s);
   net_cleanup();
   return 1;
  }

  // wait for server response to register
  char payload[2048 + 1];
  uint32_t n = 0;
  int rr = recv_frame(s, payload, 2048, &n);
  if (rr != 0) {
   fprintf(stderr, "recv_frame(register) failed\n");
   sock_close(s);
   net_cleanup();
   return 1;
  }
  payload[n] = '\0';
  printf("[server] %s\n", payload);
 }

    // run_client will send login, start recv thread, read commands from stdin
    int rc = run_client(s, user);

    net_cleanup();
    return rc;
}
