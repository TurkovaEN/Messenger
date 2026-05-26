#pragma once

// Публичный интерфейс модуля шифрования (AES-256-CBC + base64)
#include <stddef.h>

#ifdef cplusplus
// Если заголовок подключается из C++ (Qt-клиент), отключаем name mangling
extern "C" {
#endif

    // Инициализация крипто-модуля: загружает ключ из переменной окружения MSG_KEY
    int crypto_init(void);

    // Освобождение/сброс состояния модуля (в текущей реализации фактически "ничего")
    void crypto_cleanup(void);

    // Шифрует plaintext (байты) -> base64 строка (IV + ciphertext), с '\0' на конце
    // Возвращает 0 при успехе, -1 при ошибке
    int crypto_encrypt_b64(const unsigned char* plain, size_t plain_len,
        char* out_b64, size_t out_b64_sz);

    // Дешифрует base64 строку -> plaintext (как текст, добавляется '\0')
    // Возвращает 0 при успехе, -1 при ошибке
    int crypto_decrypt_b64(const char* b64,
        unsigned char* out_plain, size_t out_plain_sz,
        size_t* out_plain_len);

#ifdef cplusplus
}
#endif