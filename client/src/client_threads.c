// Модуль потоков консольного клиента:
// отдельный поток постоянно принимает сообщения от сервера,
// основной поток читает команды пользователя и отправляет их на сервер
#include "client_threads.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifndef _WIN32
// На Unix используем сигналы для прерывания блокирующего ввода (fgets)
#include <signal.h>
#endif

// Общие модули протокола/шифрования
#include "../../common/net_frame.h"
#include "../../common/kv.h"
#include "../../common/urlcodec.h"
#include "../../common/crypto.h"

// Платформенные отличия потоков: WinAPI vs pthread
#ifdef _WIN32
#include <windows.h>
typedef HANDLE thread_t;
// Прототип функции потока приёма на Windows
static DWORD WINAPI recv_thread_fn(LPVOID arg);
#else
#include <pthread.h>
typedef pthread_t thread_t;
// Прототип функции потока приёма на Unix
static void* recv_thread_fn(void* arg);
#endif

// Контекст потока приёма: сокет и общий флаг работы
typedef struct RecvCtx {
    sock_t s;
    volatile int running;
#ifndef _WIN32
    // На Unix сохраняем id основного потока, чтобы можно было прервать fgets через SIGINT
    pthread_t main_thread;
#endif
} RecvCtx;

// Шифрует plaintext payload и отправляет его как type=enc;data=...
static int send_payload(sock_t s, const char* plain_payload) {
    // Буфер под base64(IV+ciphertext)
    char b64[8192];

    // Шифруем строку и получаем base64
    if (crypto_encrypt_b64((const unsigned char*)plain_payload, strlen(plain_payload),
        b64, sizeof(b64)) != 0) {
        fprintf(stderr, "[dbg] crypto_encrypt_b64 failed\n");
        return -1;
    }

    // Формируем внешний контейнер "enc"
    char out[9000];
    int wn = snprintf(out, sizeof(out), "type=enc;data=%s", b64);
    if (wn < 0 || (size_t)wn >= sizeof(out)) {
        fprintf(stderr, "[dbg] snprintf(enc) truncated\n");
        return -1;
    }

    // Отправляем как framed payload
    int sr = send_frame(s, out, (uint32_t)strlen(out));
    if (sr != 0) {
        fprintf(stderr, "[dbg] send_frame failed, err=%d\n", net_last_error());
        return -1;
    }
    return 0;
}

// Принимает один frame, если type=enc — расшифровывает data, иначе возвращает как есть
static int recv_payload(sock_t s, char* out_plain, uint32_t out_plain_sz, uint32_t* out_len) {
    // Receive one frame, if encrypted -> decrypt, else pass through
    char buf[4600];
    uint32_t n = 0;

    // Принимаем один кадр из сети
    int rr = recv_frame(s, buf, (uint32_t)(sizeof(buf) - 1), &n);
    if (rr != 0) return rr;

    // Гарантируем нуль-терминацию
    buf[n] = '\0';

    // Проверяем поле type
    char type[32] = { 0 };
    if (!kv_get(buf, "type", type, sizeof(type))) {
        // no type, treat as plain
        if (n >= out_plain_sz) return -2;
        memcpy(out_plain, buf, n + 1);
        if (out_len) *out_len = n;
        return 0;
    }

    // Если это "enc" — расшифровываем base64 из поля data
    if (strcmp(type, "enc") == 0) {
        char data[4096] = { 0 };
        if (!kv_get(buf, "data", data, sizeof(data))) return -1;

        // Расшифровываем в out_plain
        size_t plen = 0;
        if (crypto_decrypt_b64(data, (unsigned char*)out_plain, out_plain_sz, &plen) != 0) return -1;
        if (out_len) *out_len = (uint32_t)plen;
        return 0;
    }

    // plain payload
    if (n >= out_plain_sz) return -2;
    memcpy(out_plain, buf, n + 1);
    if (out_len) *out_len = n;
    return 0;
}

// Убирает '\n' и '\r' с конца строки, чтобы команды сравнивались корректно
static void trim_newline(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

// Проверяет валидность имени комнаты (минимальная клиентская валидация)
static int validate_room_name(const char* room) {
    if (!room || !room[0]) return 0;
    // keep consistent with server MAX_ROOM_NAME (32 incl '\0')
    if (strlen(room) >= 32) return 0;
    return 1;
}

// Возвращает текущий timestamp (секунды Unix epoch)
static long long now_ts(void) {
    return (long long)time(NULL);
}

// Печатает входящее сообщение от сервера в консоль, форматируя по type=...
static void print_incoming(const char* payload) {
    // Определяем тип сообщения
    char type[32] = { 0 };
    if (!kv_get(payload, "type", type, sizeof(type))) {
        printf("[server] %s\n", payload);
        return;
    }

    // Список онлайн пользователей
    if (strcmp(type, "users") == 0) {
        char list[1600] = { 0 };
        kv_get(payload, "list", list, sizeof(list));
        printf("[users] %s\n", list[0] ? list : "<empty>");
        return;
    }

    // Элемент истории
    if (strcmp(type, "history_item") == 0) {
        char chat[32] = { 0 };
        char line_enc[1600] = { 0 };
        char line[1600] = { 0 };

        // chat=dm или chat=room
        kv_get(payload, "chat", chat, sizeof(chat));
        // line хранится в url-encoded виде
        kv_get(payload, "line", line_enc, sizeof(line_enc));

        // Декодируем строку истории
        if (url_decode(line_enc, line, sizeof(line)) != 0) {
            snprintf(line, sizeof(line), "<bad encoding>");
        }

        printf("[history %s] %s\n", chat[0] ? chat : "?", line);
        return;
    }

    // Конец истории
    if (strcmp(type, "history_end") == 0) {
        printf("[history] end\n");
        return;
    }

    // Доставка личного сообщения
    if (strcmp(type, "deliver") == 0) {
        char from[64] = { 0 };
        char text_enc[1024] = { 0 };
        char text[1024] = { 0 };

        // Достаём поля from и text
        kv_get(payload, "from", from, sizeof(from));
        kv_get(payload, "text", text_enc, sizeof(text_enc));

        // Декодируем текст
        if (url_decode(text_enc, text, sizeof(text)) != 0) {
            snprintf(text, sizeof(text), "<bad encoding>");
        }

        printf("%s: %s\n", from[0] ? from : "?", text);
        return;
    }

    // Доставка сообщения комнаты
    if (strcmp(type, "room_deliver") == 0) {
        char room[64] = { 0 };
        char from[64] = { 0 };
        char text_enc[1024] = { 0 };
        char text[1024] = { 0 };

        // Достаём room, from, text
        kv_get(payload, "room", room, sizeof(room));
        kv_get(payload, "from", from, sizeof(from));
        kv_get(payload, "text", text_enc, sizeof(text_enc));

        // Декодируем текст сообщения
        if (url_decode(text_enc, text, sizeof(text)) != 0) {
            snprintf(text, sizeof(text), "<bad encoding>");
        }

        printf("[%s] %s: %s\n", room[0] ? room : "room", from[0] ? from : "?", text);
        return;
    }

    // Информационные сообщения и ошибки
    if (strcmp(type, "info") == 0 || strcmp(type, "error") == 0) {
        char text[1024] = { 0 };
        kv_get(payload, "text", text, sizeof(text));

        // Скрываем внутренний ack delivered, чтобы не засорять чат
        if (strcmp(type, "info") == 0 && strcmp(text, "delivered") == 0) {
            return; // hide internal ack
        }

        printf("[%s] %s\n", type, text[0] ? text : payload);
        return;
    }

    // Все остальные типы выводим как сырой payload
    printf("[server] %s\n", payload);
}

// Функция потока для приема сообщений от сервера
#ifdef _WIN32
static DWORD WINAPI recv_thread_fn(LPVOID arg)
#else
static void* recv_thread_fn(void* arg)
#endif
{
    // Преобразуем переданный аргумент в указатель на контекст
    RecvCtx* ctx = (RecvCtx*)arg;
    // Буфер для хранения полученного сообщения
    char payload[2048 + 1];

    // Основной цикл работы потока
    while (ctx->running) {
        // Переменная для хранения длины полученного сообщения
        uint32_t n = 0;
        // Получаем и расшифровываем сообщение
        int rr = recv_payload(ctx->s, payload, 2048, &n);
        // Если ошибка приема значит соединение закрыто
        if (rr != 0) {
            printf("[info] connection closed\n");
            fflush(stdout);
            break;
        }

        // Добавляем завершающий нуль
        payload[n] = '\0';
        // Выводим полученное сообщение на экран
        print_incoming(payload);
        // Сбрасываем буфер вывода чтобы сообщение появилось сразу
        fflush(stdout);
    }

    // Устанавливаем флаг что поток остановлен
    ctx->running = 0;

#ifndef _WIN32
    // Interrupt blocking fgets in main thread
    pthread_kill(ctx->main_thread, SIGINT);
#endif

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

// Запускает поток приёма сообщений (обёртка над CreateThread/pthread_create)
static int start_thread(thread_t* t, RecvCtx* ctx) {
#ifdef _WIN32
    * t = CreateThread(NULL, 0, recv_thread_fn, ctx, 0, NULL);
    return *t ? 0 : -1;
#else
    return pthread_create(t, NULL, recv_thread_fn, ctx);
#endif
}

// Дожидается завершения потока приёма и освобождает ресурсы
static void join_thread(thread_t t) {
#ifdef _WIN32
    WaitForSingleObject(t, INFINITE);
    CloseHandle(t);
#else
    pthread_join(t, NULL);
#endif
}

// Основной интерактивный цикл консольного клиента
int run_client(sock_t s, const char* username) {
    // Send login
    char login_msg[256];
    snprintf(login_msg, sizeof(login_msg), "type=login;user=%s", username);

    // Логин отправляется без send_payload (как в исходнике проекта)
    if (send_frame(s, login_msg, (uint32_t)strlen(login_msg)) != 0) {
        fprintf(stderr, "send_frame(login) failed\n");
        return 1;
    }

    // Start receiver thread
    RecvCtx ctx;
    ctx.s = s;
    ctx.running = 1;
#ifndef _WIN32
    // Запоминаем основной поток для прерывания fgets()
    ctx.main_thread = pthread_self();
#endif

    // Запускаем поток приёма
    thread_t thr;
    if (start_thread(&thr, &ctx) != 0) {
        fprintf(stderr, "cannot start recv thread\n");
        return 1;
    }

    // Печатаем список доступных команд
    printf("Commands:\n");
    printf(" /msg <user> <text>\n");
    printf(" /create <room>\n");
    printf(" /join <room>\n");
    printf(" /leave <room>\n");
    printf(" /room <room> <text>\n");
    printf(" /users\n");
    printf(" /history <user>\n");
    printf(" /history_room <room>\n");
    printf(" /quit\n");

    // Буфер под ввод пользователя
    char line[1400];

    // Основной цикл: читаем stdin и отправляем команды на сервер
    while (ctx.running) {
        // Печатаем приглашение ввода
        printf("> ");
        fflush(stdout);

        // Читаем строку; при EOF выходим
        if (!fgets(line, sizeof(line), stdin)) break;

        // Убираем перевод строки
        trim_newline(line);

        // Команда выхода
        if (strcmp(line, "/quit") == 0) {
            break;
        }

        // Личное сообщение: /msg <user> <text>
        if (strncmp(line, "/msg ", 5) == 0) {
            // Пропускаем пробелы
            char* p = line + 5;
            while (*p == ' ') p++;

            // Извлекаем имя получателя
            char* to = p;
            char* sp = strchr(to, ' ');
            if (!sp) {
                printf("[error] usage: /msg <user> <text>\n");
                continue;
            }
            *sp = '\0';

            // Извлекаем текст сообщения
            char* text = sp + 1;
            while (*text == ' ') text++;

            // Валидация аргументов
            if (!to[0] || !text[0]) {
                printf("[error] usage: /msg <user> <text>\n");
                continue;
            }

            // Кодируем текст, чтобы он безопасно проходил через key=value протокол
            char text_enc[1400];
            if (url_encode(text, text_enc, sizeof(text_enc)) != 0) {
                printf("[error] message too long\n");
                continue;
            }

            // Формируем payload и отправляем
            char out[2048];
            snprintf(out, sizeof(out), "type=msg;to=%s;ts=%lld;text=%s", to, now_ts(), text_enc);
            if (send_payload(s, out) != 0) {
                printf("[error] send failed\n");
                ctx.running = 0;
                break;
            }
            continue;
        }

        // Создание комнаты: /create <room>
        if (strncmp(line, "/create ", 8) == 0) {
            char* room = line + 8;
            while (*room == ' ') room++;

            // Валидируем имя комнаты по длине
            if (!validate_room_name(room)) { printf("[error] bad room name (1..31 chars)\n"); continue; }

            // Формируем команду и отправляем
            char out[256];
            snprintf(out, sizeof(out), "type=room_create;room=%s", room);
            if (send_payload(s, out) != 0) {
                printf("[error] send failed\n");
                ctx.running = 0;
                break;
            }
            continue;
        }

        // Вход в комнату: /join <room>
        if (strncmp(line, "/join ", 6) == 0) {
            char* room = line + 6;
            while (*room == ' ') room++;

            // Проверяем имя комнаты
            if (!validate_room_name(room)) { printf("[error] bad room name (1..31 chars)\n"); continue; }

            // Отправляем join
            char out[512];
            snprintf(out, sizeof(out), "type=room_join;room=%s", room);
            if (send_payload(s, out) != 0) {
                printf("[error] send failed\n");
                ctx.running = 0;
                break;
            }
            continue;
        }

        // Выход из комнаты: /leave <room>
        if (strncmp(line, "/leave ", 7) == 0) {
            char* room = line + 7;
            while (*room == ' ') room++;

            // Проверяем имя комнаты
            if (!validate_room_name(room)) { printf("[error] bad room name (1..31 chars)\n"); continue; }

            // Отправляем leave
            char out[512];
            snprintf(out, sizeof(out), "type=room_leave;room=%s", room);
            if (send_payload(s, out) != 0) {
                printf("[error] send failed\n");
                ctx.running = 0;
                break;
            }
            continue;
        }

        // Сообщение в комнату: /room <room> <text>
        if (strncmp(line, "/room ", 6) == 0) {
            // Пропускаем пробелы
            char* p = line + 6;
            while (*p == ' ') p++;

            // Извлекаем имя комнаты
            char* room = p;
            char* sp = strchr(room, ' ');
            if (!sp) { printf("[error] usage: /room <room> <text>\n"); continue; }
            *sp = '\0';

            // Извлекаем текст
            char* text = sp + 1;
            while (*text == ' ') text++;

            // Проверяем аргументы
            if (!room[0] || !text[0]) { printf("[error] usage: /room <room> <text>\n"); continue; }

            // Кодируем текст
            char text_enc[1400];
            if (url_encode(text, text_enc, sizeof(text_enc)) != 0) {
                printf("[error] message too long\n");
                continue;
            }

            // Формируем payload room_msg
            char out[2048];
            snprintf(out, sizeof(out), "type=room_msg;room=%s;ts=%lld;text=%s", room, now_ts(), text_enc);
            if (send_payload(s, out) != 0) {
                printf("[error] send failed\n");
                ctx.running = 0;
                break;
            }
            continue;
        }

        // Запрос списка онлайн пользователей: /users
        if (strcmp(line, "/users") == 0) {
            const char* out = "type=users";
            if (send_payload(s, out) != 0) {
                printf("[error] send failed\n");
                ctx.running = 0;
                break;
            }
            continue;
        }

        // История комнаты: /history_room <room>
        if (strncmp(line, "/history_room ", 14) == 0) {
            char* room = line + 14;
            while (*room == ' ') room++;
            if (!room[0]) { printf("[error] usage: /history_room <room>\n"); continue; }

            // Формируем запрос истории и отправляем
            char out[512];
            snprintf(out, sizeof(out), "type=history_room;room=%s;limit=20", room);
            if (send_payload(s, out) != 0) {
                printf("[error] send failed\n");
                ctx.running = 0;
                break;
            }
            continue;
        }

        // История личного чата: /history <user>
        if (strncmp(line, "/history ", 9) == 0) {
            char* peer = line + 9;
            while (*peer == ' ') peer++;
            if (!peer[0]) { printf("[error] usage: /history <user>\n"); continue; }

            // Формируем запрос истории и отправляем
            char out[512];
            snprintf(out, sizeof(out), "type=history_dm;peer=%s;limit=20", peer);
            if (send_payload(s, out) != 0) {
                printf("[error] send failed\n");
                ctx.running = 0;
                break;
            }
            continue;
        }

        // Неизвестная команда
        printf("[error] unknown command\n");
    }

    // Останавливаем поток приёма
    ctx.running = 0;

    // closing socket will also unblock recv thread
    sock_close(s);

    // Дожидаемся завершения потока
    join_thread(thr);
    return 0;
}