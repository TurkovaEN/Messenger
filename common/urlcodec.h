#pragma once

// Интерфейс percent-encoding (URL-encoding) для строк:
// используется чтобы безопасно передавать текст внутри "key=value;..." протокола
#include <stddef.h>

// Encodes string to percent-encoding. Returns 0 on success, -1 if out buffer too small.
int url_encode(const char* in, char* out, size_t out_sz);

// Decodes percent-encoding. Returns 0 on success, -1 on malformed input / out buffer too small.
int url_decode(const char* in, char* out, size_t out_sz);