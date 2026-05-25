#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../common/platform.h"
#include "../../common/net_frame.h"
#include "../../common/crypto.h"
#include "../../common/kv.h"
#include "client_threads.h"

static int send_payload(sock_t s, const char* plain_payload) {
    // Encrypt plain_payload and send as type=enc;data=...
    char b64[4096];
    if (crypto_encrypt_b64((const unsigned char*)plain_payload, strlen(plain_payload),
                           b64, sizeof(b64)) != 0) {
        return -1;
    }
    char out[4600];
    snprintf(out, sizeof(out), "type=enc;data=%s", b64);
    return send_frame(s, out, (uint32_t)strlen(out));
}

static int recv_payload(sock_t s, char* out_plain, uint32_t out_plain_sz, uint32_t* out_len) {
    // Receive one frame, if encrypted -> decrypt, else pass through
    char buf[4600];
    uint32_t n = 0;
    int rr = recv_frame(s, buf, (uint32_t)(sizeof(buf) - 1), &n);
    if (rr != 0) return rr;
    buf[n] = '\0';

    char type[32] = {0};
    if (!kv_get(buf, "type", type, sizeof(type))) {
        // no type, treat as plain
        if (n >= out_plain_sz) return -2;
        memcpy(out_plain, buf, n + 1);
        if (out_len) *out_len = n;
        return 0;
    }

    if (strcmp(type, "enc") == 0) {
        char data[4096] = {0};
        if (!kv_get(buf, "data", data, sizeof(data))) return -1;
        size_t plen = 0;
        if (crypto_decrypt_b64(data, (unsigned char*)out_plain, out_plain_sz, &plen) != 0) return -1;
        if (out_len) *out_len = (uint32_t)plen;
        return 0;
    }

    // plain payload
    if (n >= out_plain_sz) return -2;
    memcpy(out_plain, buf, n + 1);
    if (out_len) *out_len = n;
    return 0;
}

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
    crypto_init();

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
  if (send_payload(s, reg_msg) != 0) {
   fprintf(stderr, "send_frame(register) failed\n");
   sock_close(s);
   net_cleanup();
   return 1;
  }

  // wait for server response to register
  char payload[2048 + 1];
  uint32_t n = 0;
  int rr = recv_payload(s, payload, 2048, &n);
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

    crypto_cleanup();
    net_cleanup();
    return rc;
}
