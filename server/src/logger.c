#include "logger.h"

#include <stdarg.h>
#include <time.h>
#include <string.h>

static FILE* g_log = NULL;

int log_init(const char* path) {
    if (!path || !path[0]) return -1;
    g_log = fopen(path, "a");
    if (!g_log) return -1;
    return 0;
}

static void timestamp(char* out, size_t out_sz) {
    time_t t = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    strftime(out, out_sz, "%Y-%m-%d %H:%M:%S", &tmv);
}

void log_info(const char* fmt, ...) {
    if (!g_log) return;

    char ts[32];
    timestamp(ts, sizeof(ts));

    fprintf(g_log, "[%s] ", ts);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);

    fputc('\n', g_log);
    fflush(g_log);
}

void log_close(void) {
    if (g_log) {
        fclose(g_log);
        g_log = NULL;
    }
}
