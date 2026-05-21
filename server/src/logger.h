#pragma once
#include <stdio.h>

/** Initialize logger (append mode). Returns 0 on success. */
int log_init(const char* path);

/** Write formatted log line with timestamp. */
void log_info(const char* fmt, ...);

/** Close log file (safe to call multiple times). */
void log_close(void);
