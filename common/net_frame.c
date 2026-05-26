// Реализация простого бинарного фрейминга поверх TCP:
// сначала 4 байта длины (network byte order), затем payload указанной длины
#include "net_frame.h"
#include <string.h>

// Надёжно отправляет n байт, делая несколько send() при частичных отправках
static int send_all(sock_t s, const char* p, uint32_t n) {
    // Пока есть что отправлять — продолжаем слать
    while (n > 0) {
        // На Windows send() принимает длину как int
#ifdef _WIN32
        int sent = send(s, p, (int)n, 0);
#else
        // На Unix используем обычный send() (возвращает ssize_t, приводим к int)
        int sent = (int)send(s, p, n, 0);
#endif
        // sent <= 0 означает ошибку или разрыв соединения
        if (sent <= 0) return -1;

        // Сдвигаем указатель и уменьшаем оставшееся количество байт
        p += sent;
        n -= (uint32_t)sent;
    }

    // Все байты успешно отправлены
    return 0;
}

// Надёжно принимает n байт, делая несколько recv() при частичном чтении
static int recv_all(sock_t s, char* p, uint32_t n) {
    // Пока не прочитали все n байт — продолжаем читать
    while (n > 0) {
        // На Windows recv() принимает длину как int
#ifdef _WIN32
        int r = recv(s, p, (int)n, 0);
#else
        // На Unix используем обычный recv()
        int r = (int)recv(s, p, n, 0);
#endif
        // r <= 0 означает ошибка или закрытие соединения
        if (r <= 0) return -1; // disconnect or error

        // Сдвигаем указатель и уменьшаем счётчик оставшихся байт
        p += r;
        n -= (uint32_t)r;
    }

    // Все байты успешно получены
    return 0;
}

// Отправляет один кадр: 4 байта длины + payload
int send_frame(sock_t s, const void* data, uint32_t len) {
    // Переводим длину в сетевой порядок байт
    uint32_t net_len = htonl(len);

    // Сначала отправляем заголовок (длину)
    if (send_all(s, (const char*)&net_len, (uint32_t)sizeof(net_len)) != 0) return -1;

    // Пустой payload допустим
    if (len == 0) return 0;

    // Затем отправляем само содержимое
    return send_all(s, (const char*)data, len);
}

// Принимает один кадр: читает длину, затем payload
int recv_frame(sock_t s, void* buf, uint32_t max_len, uint32_t* out_len) {
    // Читаем 4-байтную длину
    uint32_t net_len = 0;
    if (recv_all(s, (char*)&net_len, (uint32_t)sizeof(net_len)) != 0) return -1;

    // Переводим длину из сетевого порядка байт
    uint32_t len = ntohl(net_len);

    // Возвращаем фактическую длину кадра вызывающему коду
    if (out_len) *out_len = len;

    // Нулевой payload — валидный кадр
    if (len == 0) return 0;

    // Если помещается в буфер — читаем напрямую
    if (len <= max_len) {
        if (recv_all(s, (char*)buf, len) != 0) return -1;
        return 0;
    }

    // Если payload больше буфера — «дренируем» данные, чтобы не ломать поток
    // и возвращаем -2, сигнализируя что сообщение было слишком большим
    uint32_t remaining = len;
    char tmp[512];
    while (remaining > 0) {
        uint32_t chunk = remaining > (uint32_t)sizeof(tmp) ? (uint32_t)sizeof(tmp) : remaining;
        if (recv_all(s, tmp, chunk) != 0) return -1;
        remaining -= chunk;
    }
    return -2;
}