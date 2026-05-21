#pragma once
#include <stddef.h>

/**
 * Finds value for key in string like "a=1;b=2;type=login".
 * Writes value to out (null-terminated) up to out_sz.
 * Returns 1 if found, 0 if not found.
 */
int kv_get(const char* s, const char* key, char* out, size_t out_sz);
