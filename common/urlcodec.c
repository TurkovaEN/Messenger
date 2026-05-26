// URL/percent-encoding для передачи произвольного текста внутри протокола key=value
#include "urlcodec.h"
#include <ctype.h>
#include <string.h>

// Преобразует один hex-символ в значение 0..15, либо возвращает -1 при ошибке
static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

// Кодирует строку в percent-encoding
// Безопасные символы (a-zA-Z0-9-_.~ и пробел) остаются как есть
int url_encode(const char* in, char* out, size_t out_sz) {
    // Проверяем параметры
    if (!in || !out || out_sz == 0) return -1;

    // Индекс записи в выходной буфер
    size_t w = 0;

    // Идём по входной строке до '\0'
    for (size_t i = 0; in[i]; i++) {
        unsigned char c = (unsigned char)in[i];

        // Проверяем, можно ли передавать символ без кодирования
        int safe =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~' || c == ' ';

        if (safe) {
            // Для обычного символа нужен 1 байт места
            if (w + 1 >= out_sz) return -1;
            out[w++] = (char)c;
        }
        else {
            // Для %XX нужно 3 байта
            if (w + 3 >= out_sz) return -1;
            static const char* H = "0123456789ABCDEF";
            out[w++] = '%';
            out[w++] = H[(c >> 4) & 0xF];
            out[w++] = H[c & 0xF];
        }
    }

    // Добавляем завершающий нуль
    if (w >= out_sz) return -1;
    out[w] = '\0';
    return 0;
}

// Декодирует percent-encoding обратно в исходную строку
int url_decode(const char* in, char* out, size_t out_sz) {
    // Проверяем параметры
    if (!in || !out || out_sz == 0) return -1;

    // Индекс записи результата
    size_t w = 0;

    // Проходим по входу
    for (size_t i = 0; in[i]; i++) {
        char c = in[i];

        // Последовательность вида %AB
        if (c == '%') {
            // Должно быть минимум ещё 2 символа
            if (!in[i + 1] || !in[i + 2]) return -1;

            // Конвертируем hex-пару
            int hi = hexval(in[i + 1]);
            int lo = hexval(in[i + 2]);
            if (hi < 0 || lo < 0) return -1;

            // Записываем один байт
            unsigned char v = (unsigned char)((hi << 4) | lo);
            if (w + 1 >= out_sz) return -1;
            out[w++] = (char)v;

            // Пропускаем два hex-символа
            i += 2;
        }
        else {
            // Обычный символ копируем напрямую
            if (w + 1 >= out_sz) return -1;
            out[w++] = c;
        }
    }

    // Завершаем строку
    out[w] = '\0';
    return 0;
}