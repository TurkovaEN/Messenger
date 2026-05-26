#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

// Простой модуль логирования сервера в файл
#include "logger.h"
#include <stdarg.h>
#include <time.h>
#include <string.h>

// Глобальный дескриптор лог-файла (один на весь процесс)
static FILE* g_log = NULL;

// Открывает лог-файл в режиме добавления (append)
int log_init(const char* path) {
    // Проверяем аргумент пути
    if (!path || !path[0]) return -1;

    // Открываем файл для добавления
    g_log = fopen(path, "a");
    if (!g_log) return -1;

    return 0;
}

// Формирует строку с текущим временем для логов
static void timestamp(char* out, size_t out_sz) {
    // Получаем текущее время
    time_t t = time(NULL);
    struct tm tmv;

    // Потокобезопасное преобразование time_t -> localtime
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif

    // Форматируем время: YYYY-MM-DD HH:MM:SS
    strftime(out, out_sz, "%Y-%m-%d %H:%M:%S", &tmv);
}

// Записывает одну строку в лог с timestamp и форматированием printf-подобным образом
void log_info(const char* fmt, ...) {
    // Если лог не открыт, то ничего не делаем
    if (!g_log) return;

    // Печатаем префикс времени
    char ts[32];
    timestamp(ts, sizeof(ts));
    fprintf(g_log, "[%s] ", ts);

    // Печатаем сообщение по формату
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);

    // Завершаем строку и сбрасываем буфер
    fputc('\n', g_log);
    fflush(g_log);
}

// Закрывает лог-файл и очищает глобальный указатель
void log_close(void) {
    if (g_log) {
        fclose(g_log);
        g_log = NULL;
    }
}