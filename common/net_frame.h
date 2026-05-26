#pragma once

// Заголовок модуля фрейминга TCP-сообщений: 4 байта длины + payload
#include <stdint.h>
#include <stddef.h>

// Используем sock_t и кроссплатформенные include для сокетов
#include "platform.h"

/**
 * Sends one frame: [uint32 length in network byte order] + payload bytes.
 * Returns 0 on success, -1 on error.
 */
int send_frame(sock_t s, const void* data, uint32_t len);

/**
 * Receives one frame into buffer (up to max_len bytes).
 * On success returns 0 and sets *out_len to payload length.
 * Returns:
 * -1 on socket error / disconnect,
 * -2 if incoming payload is larger than max_len (frame will be drained).
 */
int recv_frame(sock_t s, void* buf, uint32_t max_len, uint32_t* out_len);