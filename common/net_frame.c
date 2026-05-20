#include "net_frame.h"

#include <string.h>

static int send_all(sock_t s, const char* p, uint32_t n) {
    while (n > 0) {
#ifdef _WIN32
        int sent = send(s, p, (int)n, 0);
#else
        int sent = (int)send(s, p, n, 0);
#endif
        if (sent <= 0) return -1;
        p += sent;
        n -= (uint32_t)sent;
    }
    return 0;
}

static int recv_all(sock_t s, char* p, uint32_t n) {
    while (n > 0) {
#ifdef _WIN32
        int r = recv(s, p, (int)n, 0);
#else
        int r = (int)recv(s, p, n, 0);
#endif
        if (r <= 0) return -1; // disconnect or error
        p += r;
        n -= (uint32_t)r;
    }
    return 0;
}

int send_frame(sock_t s, const void* data, uint32_t len) {
    uint32_t net_len = htonl(len);
    if (send_all(s, (const char*)&net_len, (uint32_t)sizeof(net_len)) != 0) return -1;
    if (len == 0) return 0;
    return send_all(s, (const char*)data, len);
}

int recv_frame(sock_t s, void* buf, uint32_t max_len, uint32_t* out_len) {
    uint32_t net_len = 0;
    if (recv_all(s, (char*)&net_len, (uint32_t)sizeof(net_len)) != 0) return -1;

    uint32_t len = ntohl(net_len);
    if (out_len) *out_len = len;

    if (len == 0) return 0;

    if (len <= max_len) {
        if (recv_all(s, (char*)buf, len) != 0) return -1;
        return 0;
    }

    // Payload too big: drain it to keep stream consistent
    uint32_t remaining = len;
    char tmp[512];
    while (remaining > 0) {
        uint32_t chunk = remaining > (uint32_t)sizeof(tmp) ? (uint32_t)sizeof(tmp) : remaining;
        if (recv_all(s, tmp, chunk) != 0) return -1;
        remaining -= chunk;
    }
    return -2;
}
