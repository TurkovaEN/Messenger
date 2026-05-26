// Кроссплатформенная реализация сетевых примитивов для Windows и Unix
#include "platform.h"

#ifdef _WIN32

// Инициализация Winsock на Windows (без этого socket/connect работать не будут)
int net_init(void) {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa);
}

// Завершение работы Winsock
void net_cleanup(void) {
    WSACleanup();
}

// Закрытие сокета (Windows-версия)
int sock_close(sock_t s) {
    return closesocket(s);
}

// Получение кода последней сетевой ошибки (Windows-версия)
int net_last_error(void) {
    return WSAGetLastError();
}

#else

// На Unix дополнительная инициализация сети не требуется
int net_init(void) { return 0; }

// На Unix инициализация не нужна, поэтому cleanup пустой
void net_cleanup(void) {}

// Закрытие сокета (Unix-версия)
int sock_close(sock_t s) { return close(s); }

// Получение кода последней сетевой ошибки (Unix-версия)
int net_last_error(void) { return errno; }

#endif