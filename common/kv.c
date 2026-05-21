#include "kv.h"
#include <string.h>

int kv_get(const char* s, const char* key, char* out, size_t out_sz) {
    if (!s || !key || !out || out_sz == 0) return 0;

    size_t klen = strlen(key);
    const char* p = s;

    while (*p) {
        // token: key=value (ends by ';' or end)
        const char* eq = strchr(p, '=');
        if (!eq) break;

        size_t cur_klen = (size_t)(eq - p);
        if (cur_klen == klen && strncmp(p, key, klen) == 0) {
            const char* val = eq + 1;
            const char* end = strchr(val, ';');
            size_t vlen = end ? (size_t)(end - val) : strlen(val);
            if (vlen >= out_sz) vlen = out_sz - 1;
            memcpy(out, val, vlen);
            out[vlen] = '\0';
            return 1;
        }

        const char* semi = strchr(p, ';');
        if (!semi) break;
        p = semi + 1;
    }
    return 0;
}
