#pragma once

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET sock_t;
  #define SOCK_INVALID INVALID_SOCKET
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <errno.h>
  typedef int sock_t;
  #define SOCK_INVALID (-1)
#endif

int net_init(void);
void net_cleanup(void);
int sock_close(sock_t s);
int net_last_error(void);
