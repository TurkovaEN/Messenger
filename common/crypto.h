#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize crypto module (loads key from env, prepares OpenSSL if needed)
int crypto_init(void);
void crypto_cleanup(void);

// Encrypt plain bytes -> base64 string (null-terminated)
// Returns 0 on success, -1 on error
int crypto_encrypt_b64(const unsigned char* plain, size_t plain_len,
                       char* out_b64, size_t out_b64_sz);

// Decrypt base64 string -> plain bytes (null-terminated as text)
// Returns 0 on success, -1 on error
int crypto_decrypt_b64(const char* b64,
                       unsigned char* out_plain, size_t out_plain_sz,
                       size_t* out_plain_len);

#ifdef __cplusplus
}
#endif
