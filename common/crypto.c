#define _CRT_SECURE_NO_WARNINGS

// Реализация шифрования транспортных сообщений:
// AES-256-CBC, случайный IV на сообщение, результат кодируется в base64
#include "crypto.h"
#include <string.h>
#include <stdlib.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

// Размеры ключа и IV для AES-256-CBC
#define KEY_LEN 32 // AES-256
#define IV_LEN 16  // CBC IV

// Глобальный ключ (загружается один раз при init)
static unsigned char g_key[KEY_LEN];
// Флаг инициализации модуля
static int g_inited = 0;

// Загружает ключ из переменной окружения MSG_KEY или использует demo-значение
static void load_key_from_env(void) {
    // Читаем переменную окружения
    const char* env = getenv("MSG_KEY");

    // If env not set, use deterministic default (for demo). In report mention it.
    if (!env || !env[0]) {
        const char* def = "default_demo_key_change_me_32bytes!";
        // Ensure 32 bytes: pad/trim
        memset(g_key, 0, sizeof(g_key));
        memcpy(g_key, def, strlen(def) > KEY_LEN ? KEY_LEN : strlen(def));
        return;
    }

    // Use first 32 bytes of env (ASCII). If shorter - zero pad.
    memset(g_key, 0, sizeof(g_key));
    size_t n = strlen(env);
    if (n > KEY_LEN) n = KEY_LEN;
    memcpy(g_key, env, n);
}

// Инициализация крипто-модуля (загрузка ключа)
int crypto_init(void) {
    // Не инициализируем повторно
    if (g_inited) return 0;

    // Загружаем ключ
    load_key_from_env();
    g_inited = 1;
    return 0;
}

// Завершение работы модуля
void crypto_cleanup(void) {
    // nothing
    g_inited = 0;
}

// Base64 helpers using EVP_EncodeBlock / EVP_DecodeBlock

// Кодирует байты в base64, возвращает число записанных символов или -1 при ошибке
static int b64_encode(const unsigned char* in, int inlen, char* out, size_t outsz) {
    // Output length is 4*ceil(n/3) + 1
    int need = 4 * ((inlen + 2) / 3);
    if ((size_t)need + 1 > outsz) return -1;

    // EVP_EncodeBlock пишет без '\0', добавляем его сами
    int written = EVP_EncodeBlock((unsigned char*)out, in, inlen);
    if (written < 0) return -1;
    out[written] = '\0';
    return written;
}

// Декодирует base64 в байты, возвращает 0 при успехе или -1 при ошибке
static int b64_decode(const char* in, unsigned char* out, size_t outsz, int* outlen) {
    // Длина входной base64 строки
    int inlen = (int)strlen(in);

    // decoded len <= 3*(inlen/4)
    int need = 3 * (inlen / 4);
    if ((size_t)need + 1 > outsz) return -1;

    // Декодируем base64
    int written = EVP_DecodeBlock(out, (const unsigned char*)in, inlen);
    if (written < 0) return -1;

    // EVP_DecodeBlock включает паддинг в подсчёт; корректируем по '='
    int pad = 0;
    if (inlen >= 1 && in[inlen - 1] == '=') pad++;
    if (inlen >= 2 && in[inlen - 2] == '=') pad++;
    written -= pad;

    // Делаем вывод нуль-терминированным (удобно для текстовых payload)
    out[written] = '\0';
    if (outlen) *outlen = written;
    return 0;
}

// Шифрует plaintext и кодирует результат (IV + ciphertext) в base64
int crypto_encrypt_b64(const unsigned char* plain, size_t plain_len,
    char* out_b64, size_t out_b64_sz) {
    // Авто-инициализация при первом использовании
    if (!g_inited) crypto_init();

    // Проверяем параметры
    if (!plain || plain_len == 0 || !out_b64 || out_b64_sz == 0) return -1;

    // Генерируем случайный IV
    unsigned char iv[IV_LEN];
    if (RAND_bytes(iv, sizeof(iv)) != 1) return -1;

    // Создаём контекст OpenSSL для EVP-шифрования
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;

    int rc = -1;
    unsigned char* cipher = NULL;

    // ciphertext length <= plain_len + block_size
    int max_cipher = (int)plain_len + 16;
    cipher = (unsigned char*)malloc((size_t)max_cipher);
    if (!cipher) goto done;

    // Шифруем: Init -> Update -> Final
    int outl1 = 0, outl2 = 0;
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, g_key, iv) != 1) goto done;
    if (EVP_EncryptUpdate(ctx, cipher, &outl1, plain, (int)plain_len) != 1) goto done;
    if (EVP_EncryptFinal_ex(ctx, cipher + outl1, &outl2) != 1) goto done;

    // Итоговая длина шифротекста
    int cipher_len = outl1 + outl2;

    // We will base64( IV || CIPHERTEXT )
    size_t blob_len = IV_LEN + (size_t)cipher_len;
    unsigned char* blob = (unsigned char*)malloc(blob_len);
    if (!blob) goto done;

    // Склеиваем IV и ciphertext в один блок
    memcpy(blob, iv, IV_LEN);
    memcpy(blob + IV_LEN, cipher, (size_t)cipher_len);

    // Кодируем склеенный блок в base64
    if (b64_encode(blob, (int)blob_len, out_b64, out_b64_sz) < 0) {
        free(blob);
        goto done;
    }

    // Освобождаем временный буфер
    free(blob);
    rc = 0;

done:
    // Освобождаем ресурсы шифрования
    if (cipher) free(cipher);
    EVP_CIPHER_CTX_free(ctx);
    return rc;
}

// Расшифровывает base64(IV + ciphertext) обратно в plaintext
int crypto_decrypt_b64(const char* b64,
    unsigned char* out_plain, size_t out_plain_sz,
    size_t* out_plain_len) {
    // Авто-инициализация при первом использовании
    if (!g_inited) crypto_init();

    // Проверяем параметры
    if (!b64 || !b64[0] || !out_plain || out_plain_sz == 0) return -1;

    // Выделяем буфер под decoded blob (используем out_plain_sz как верхнюю границу)
    unsigned char* blob = (unsigned char*)malloc(out_plain_sz);
    if (!blob) return -1;

    // Декодируем base64
    int blob_len = 0;
    if (b64_decode(b64, blob, out_plain_sz, &blob_len) != 0) {
        free(blob);
        return -1;
    }

    // Минимально должен присутствовать IV
    if (blob_len < IV_LEN) {
        free(blob);
        return -1;
    }

    // Извлекаем IV и указатель на ciphertext
    unsigned char iv[IV_LEN];
    memcpy(iv, blob, IV_LEN);
    unsigned char* cipher = blob + IV_LEN;
    int cipher_len = blob_len - IV_LEN;

    // Создаём контекст для расшифровки
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) { free(blob); return -1; }

    int rc = -1;
    int outl1 = 0, outl2 = 0;

    // Инициализируем AES-256-CBC для расшифровки
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, g_key, iv) != 1) goto done;

    // Проверяем, что в out_plain поместится результат
    if ((int)out_plain_sz < cipher_len + 1) goto done;

    // Расшифровка: Update -> Final
    if (EVP_DecryptUpdate(ctx, out_plain, &outl1, cipher, cipher_len) != 1) goto done;
    if (EVP_DecryptFinal_ex(ctx, out_plain + outl1, &outl2) != 1) goto done;

    // Записываем длину и добавляем '\0' как у текстовой строки
    {
        size_t plen = (size_t)(outl1 + outl2);
        if (plen >= out_plain_sz) goto done;
        out_plain[plen] = '\0';
        if (out_plain_len) *out_plain_len = plen;
    }

    rc = 0;

done:
    // Освобождаем ресурсы
    EVP_CIPHER_CTX_free(ctx);
    free(blob);
    return rc;
}