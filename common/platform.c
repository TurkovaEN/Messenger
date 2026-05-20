#include "platform.h"

#ifdef _WIN32
int net_init(void) {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2,2), &wsa);
}
void net_cleanup(void) {
    WSACleanup();
}
int sock_close(sock_t s) {
    return closesocket(s);
}
int net_last_error(void) {
    return WSAGetLastError();
}
#else
int net_init(void) { return 0; }
void net_cleanup(void) {}
int sock_close(sock_t s) { return close(s); }
int net_last_error(void) { return errno; }
#endif
