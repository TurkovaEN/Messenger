#pragma once

// Кроссплатформенный заголовок для сокетов:
// на Windows подключаем Winsock2, на Unix — POSIX sockets и errno
#ifdef _WIN32

// Ускоряет сборку Windows-заголовков, исключая редко используемые части
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Winsock2/WS2TCPIP для socket(), connect(), inet_pton() и т.п.
#include <winsock2.h>
#include <ws2tcpip.h>

// На Windows тип сокета — SOCKET
typedef SOCKET sock_t;

// Маркер невалидного сокета
#define SOCK_INVALID INVALID_SOCKET

#else

// POSIX заголовки для сокетов
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// close()
#include <unistd.h>

// errno
#include <errno.h>

// На Unix тип сокета — int (файловый дескриптор)
typedef int sock_t;

// Маркер невалидного сокета
#define SOCK_INVALID (-1)

#endif

// Инициализация сетевого слоя (WSAStartup на Windows, noop на Unix)
int net_init(void);

// Очистка сетевого слоя (WSACleanup на Windows, noop на Unix)
void net_cleanup(void);

// Закрытие сокета (closesocket/close)
int sock_close(sock_t s);

// Получение кода последней сетевой ошибки (WSAGetLastError/errno)
int net_last_error(void);