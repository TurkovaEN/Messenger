// Реализация простого парсера строк формата "key=value;key2=value2"
#include "kv.h"
#include <string.h>

// Ищет значение по ключу в строке вида "a=1;b=2;type=login"
// Возвращает 1 если ключ найден, иначе 0
int kv_get(const char* s, const char* key, char* out, size_t out_sz) {
    // Проверяем корректность входных параметров
    if (!s || !key || !out || out_sz == 0) return 0;

    // Длина искомого ключа
    size_t klen = strlen(key);
    // Указатель на текущую позицию разбора
    const char* p = s;

    // Последовательно перебираем токены, разделённые ';'
    while (*p) {
        // Токен ожидается в виде key=value
        const char* eq = strchr(p, '=');
        if (!eq) break;

        // Длина текущего ключа (до '=')
        size_t cur_klen = (size_t)(eq - p);

        // Сравниваем ключи по длине и содержимому
        if (cur_klen == klen && strncmp(p, key, klen) == 0) {
            // Указатель на значение сразу после '='
            const char* val = eq + 1;
            // Ищем конец значения (до ';' или конец строки)
            const char* end = strchr(val, ';');
            size_t vlen = end ? (size_t)(end - val) : strlen(val);

            // Обрезаем по размеру буфера вывода, чтобы не выйти за границы
            if (vlen >= out_sz) vlen = out_sz - 1;

            // Копируем значение и добавляем завершающий нуль
            memcpy(out, val, vlen);
            out[vlen] = '\0';
            return 1;
        }

        // Если ключ не совпал — переходим к следующему токену после ';'
        const char* semi = strchr(p, ';');
        if (!semi) break;
        p = semi + 1;
    }

    // Ключ не найден
    return 0;
}