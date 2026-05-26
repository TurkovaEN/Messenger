#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define strdup _strdup
#endif

// Основной файл сервера: сетевой цикл на select(), обработка команд протокола,
// хранение списка клиентов, комнат и запись истории в файлы
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../common/urlcodec.h"
#include "../../common/net_frame.h"
#include "../../common/kv.h"
#include "../../common/crypto.h"
#include "logger.h"
#include <signal.h>

// Максимальные размеры/лимиты (простые фиксированные массивы для учебного проекта)
#define MAX_CLIENTS 64
#define MAX_USER 32
#define MAX_PAYLOAD 2048
#define MAX_ROOMS 32
#define MAX_ROOM_NAME 32
#define MAX_ROOM_MEMBERS 64

// Глобальный флаг остановки сервера по сигналам
static volatile sig_atomic_t g_stop = 0;

// Обработчик SIGINT/SIGTERM: устанавливает флаг, основной цикл завершится сам
static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

// Структура подключенного клиента
typedef struct Client {
    sock_t sock;
    int used;
    char user[MAX_USER];
} Client;

// Структура комнаты: имя и список индексов клиентов (в массиве clients[])
typedef struct Room {
    int used;
    char name[MAX_ROOM_NAME];
    int members[MAX_ROOM_MEMBERS]; // indexes in clients[]
    int member_count;
} Room;

// Отправляет payload в зашифрованном виде (type=enc;data=base64)
static int send_payload(sock_t s, const char* plain_payload) {
    // Encrypt plain_payload and send as type=enc;data=...
    char b64[4096];

    // Шифруем plaintext и получаем base64
    if (crypto_encrypt_b64((const unsigned char*)plain_payload, strlen(plain_payload),
        b64, sizeof(b64)) != 0) {
        return -1;
    }

    // Формируем внешний протокольный контейнер "type=enc"
    char out[4600];
    snprintf(out, sizeof(out), "type=enc;data=%s", b64);

    // Отправляем как framed-message
    return send_frame(s, out, (uint32_t)strlen(out));
}

// Принимает один кадр и, если он зашифрован (type=enc), расшифровывает его
static int recv_payload(sock_t s, char* out_plain, uint32_t out_plain_sz, uint32_t* out_len) {
    // Receive one frame, if encrypted -> decrypt, else pass through
    char buf[4600];
    uint32_t n = 0;

    // Читаем один frame из сети
    int rr = recv_frame(s, buf, (uint32_t)(sizeof(buf) - 1), &n);
    if (rr != 0) return rr;

    // Делаем buf нуль-терминированной строкой
    buf[n] = '\0';

    // Определяем тип сообщения
    char type[32] = { 0 };
    if (!kv_get(buf, "type", type, sizeof(type))) {
        // Если нет поля type — считаем это plaintext (для совместимости/отладки)
        if (n >= out_plain_sz) return -2;
        memcpy(out_plain, buf, n + 1);
        if (out_len) *out_len = n;
        return 0;
    }

    // Если внешний тип enc — расшифровываем поле data
    if (strcmp(type, "enc") == 0) {
        char data[4096] = { 0 };
        if (!kv_get(buf, "data", data, sizeof(data))) return -1;

        // Дешифруем base64 -> plaintext
        size_t plen = 0;
        if (crypto_decrypt_b64(data, (unsigned char*)out_plain, out_plain_sz, &plen) != 0) return -1;
        if (out_len) *out_len = (uint32_t)plen;
        return 0;
    }

    // Иначе сообщение не зашифровано — копируем как есть
    if (n >= out_plain_sz) return -2;
    memcpy(out_plain, buf, n + 1);
    if (out_len) *out_len = n;
    return 0;
}

// Печатает usage для запуска сервера
static void usage(const char* prog) {
    fprintf(stderr, "Usage: %s <port> <log_path> [users_path]\n", prog);
}

// Добавляет сокет в массив клиентов, возвращает индекс слота или -1 если мест нет
static int add_client(Client* clients, sock_t s) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].used) {
            clients[i].used = 1;
            clients[i].sock = s;
            clients[i].user[0] = '\0';
            return i;
        }
    }
    return -1;
}

// Удаляет клиента из массива: закрывает сокет и очищает слот
static void remove_client(Client* clients, int idx) {
    if (idx < 0 || idx >= MAX_CLIENTS) return;
    if (clients[idx].used) {
        sock_close(clients[idx].sock);
        clients[idx].used = 0;
        clients[idx].sock = SOCK_INVALID;
        clients[idx].user[0] = '\0';
    }
}

// Ищет онлайн-клиента по имени пользователя, возвращает индекс или -1
static int find_by_user(Client* clients, const char* user) {
    if (!user || !user[0]) return -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].used && clients[i].user[0] && strcmp(clients[i].user, user) == 0) {
            return i;
        }
    }
    return -1;
}

// Ищет комнату по имени, возвращает индекс или -1
static int find_room(Room* rooms, const char* name) {
    if (!name || !name[0]) return -1;
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].used && strcmp(rooms[i].name, name) == 0) return i;
    }
    return -1;
}

// Создаёт комнату в массиве rooms, возвращает индекс, -2 если уже существует, -1 если нет мест
static int add_room(Room* rooms, const char* name) {
    if (!name || !name[0]) return -1;
    if (find_room(rooms, name) >= 0) return -2; // already exists

    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].used) {
            rooms[i].used = 1;
            strncpy(rooms[i].name, name, sizeof(rooms[i].name) - 1);
            rooms[i].name[sizeof(rooms[i].name) - 1] = '\0';
            rooms[i].member_count = 0;
            return i;
        }
    }
    return -1; // no slots
}

// Проверяет, состоит ли client_idx в комнате
static int room_has_member(const Room* r, int client_idx) {
    for (int i = 0; i < r->member_count; i++) {
        if (r->members[i] == client_idx) return 1;
    }
    return 0;
}

// Добавляет клиента в комнату, если есть место, возвращает 0 или -1 если комната заполнена
static int room_add_member(Room* r, int client_idx) {
    if (r->member_count >= MAX_ROOM_MEMBERS) return -1;
    if (room_has_member(r, client_idx)) return 0;
    r->members[r->member_count++] = client_idx;
    return 0;
}

// Удаляет клиента из комнаты (если найден), делая swap-remove
static void room_remove_member(Room* r, int client_idx) {
    for (int i = 0; i < r->member_count; i++) {
        if (r->members[i] == client_idx) {
            r->members[i] = r->members[r->member_count - 1];
            r->member_count--;
            return;
        }
    }
}

// Рассылает сообщение всем участникам комнаты (опционально пропуская одного клиента)
static void room_broadcast(Room* r, Client* clients, const char* msg, int skip_idx) {
    for (int i = 0; i < r->member_count; i++) {
        int idx = r->members[i];
        if (idx == skip_idx) continue;
        if (!clients[idx].used) continue;

        // Ошибки отправки тут игнорируются: дисконнект будет обработан основным циклом
        if (send_payload(clients[idx].sock, msg) != 0) {
            // ignore errors here; disconnects will be handled by main loop
        }
    }
}

// Загружает список комнат из файла (по одной комнате на строку)
static int rooms_load(Room* rooms, const char* path) {
    if (!path || !path[0]) return -1;
    FILE* f = fopen(path, "r");
    if (!f) return -1;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        // Убираем CR/LF
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) { line[n - 1] = '\0'; n--; }
        if (line[0] == '\0') continue;

        // Создаём комнаты; ошибки (дубликаты/переполнение) игнорируем
        add_room(rooms, line);
    }

    fclose(f);
    return 0;
}

// Добавляет комнату в файл rooms.txt (append)
static int rooms_append(const char* path, const char* room) {
    if (!path || !path[0] || !room || !room[0]) return -1;
    FILE* f = fopen(path, "a");
    if (!f) return -1;
    fprintf(f, "%s\n", room);
    fclose(f);
    return 0;
}

// Гарантирует существование users-файла (создаёт при необходимости)
static int users_ensure_file(const char* path) {
    FILE* f = fopen(path, "a");
    if (!f) return -1;
    fclose(f);
    return 0;
}

// Проверяет, зарегистрирован ли пользователь в users.txt
static int users_exists(const char* path, const char* user) {
    if (!path || !user || !user[0]) return 0;
    FILE* f = fopen(path, "r");
    if (!f) return 0;

    char line[128];
    while (fgets(line, sizeof(line), f)) {
        // Убираем CR/LF
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) { line[n - 1] = '\0'; n--; }

        // Сравниваем имя целиком
        if (strcmp(line, user) == 0) { fclose(f); return 1; }
    }

    fclose(f);
    return 0;
}

// Добавляет пользователя в users.txt
static int users_append(const char* path, const char* user) {
    if (!path || !user || !user[0]) return -1;
    FILE* f = fopen(path, "a");
    if (!f) return -1;
    fprintf(f, "%s\n", user);
    fclose(f);
    return 0;
}

// Сортирует две строки лексикографически (используется для имени файла dm_*.txt)
static void sort2(const char** a, const char** b) {
    if (strcmp(*a, *b) > 0) {
        const char* t = *a;
        *a = *b;
        *b = t;
    }
}

// Формирует путь к истории личных сообщений dm_u1_u2.txt (пары сортируются, чтобы имя было стабильным)
static void history_path_dm(char* out, size_t out_sz, const char* u1, const char* u2) {
    const char* a = u1;
    const char* b = u2;
    sort2(&a, &b);
    snprintf(out, out_sz, "server/history/dm_%s_%s.txt", a, b);
}

// Формирует путь к истории комнаты room_<name>.txt
static void history_path_room(char* out, size_t out_sz, const char* room) {
    snprintf(out, out_sz, "server/history/room_%s.txt", room);
}

// Добавляет строку в файл истории (append)
static void history_append_line(const char* path, const char* line) {
    FILE* f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%s\n", line);
    fclose(f);
}

// Отправляет последние limit строк истории, используя ring buffer фиксированного размера
static int history_send_last_lines(sock_t s, const char* chat, const char* path, int limit) {
    // Ограничиваем limit разумными рамками
    if (limit <= 0) limit = 20;
    if (limit > 200) limit = 200;

    // Если файла нет — просто завершаем историю
    FILE* f = fopen(path, "r");
    if (!f) {
        const char* end = "type=history_end";
        send_frame(s, end, (uint32_t)strlen(end));
        return 0;
    }

    // Read all lines into a ring buffer (simple and OK for small history)
    char** lines = (char**)calloc((size_t)limit, sizeof(char*));
    if (!lines) { fclose(f); return -1; }

    int count = 0;
    int pos = 0;

    char buf[1600];
    while (fgets(buf, sizeof(buf), f)) {
        // Убираем CR/LF
        size_t n = strlen(buf);
        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) { buf[n - 1] = '\0'; n--; }

        // Перезаписываем текущую позицию в кольце
        free(lines[pos]);
        lines[pos] = strdup(buf);
        if (!lines[pos]) { /* ignore OOM for this line */ }

        // Двигаем позицию и увеличиваем счётчик заполнения
        pos = (pos + 1) % limit;
        if (count < limit) count++;
    }
    fclose(f);

    // Send in correct order: oldest -> newest
    int start = (count == limit) ? pos : 0;
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % limit;
        if (!lines[idx]) continue;

        // Кодируем строку для передачи внутри key=value
        char line_net[1800];
        if (url_encode(lines[idx], line_net, sizeof(line_net)) != 0) continue;

        // Отправляем history_item
        char out[2048];
        snprintf(out, sizeof(out), "type=history_item;chat=%s;line=%s", chat, line_net);
        send_frame(s, out, (uint32_t)strlen(out));
    }

    // Освобождаем кольцевой буфер
    for (int i = 0; i < limit; i++) free(lines[i]);
    free(lines);

    // Сообщаем конец истории
    const char* end = "type=history_end";
    send_frame(s, end, (uint32_t)strlen(end));
    return 0;
}

// Обрабатывает одну команду/кадр протокола от клиента
static int handle_frame(Client* clients, Room* rooms, const char* users_path, const char* rooms_path,
    int client_idx, Client* c, const char* payload) {
    // Достаём type из строки key=value;...
    char type[32];
    if (!kv_get(payload, "type", type, sizeof(type))) {
        return 0;
    }

    // Регистрация нового пользователя
    if (strcmp(type, "register") == 0) {
        char user[MAX_USER];
        if (!kv_get(payload, "user", user, sizeof(user))) {
            const char* err = "type=error;text=missing user";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Запрещаем перерегистрацию
        if (users_exists(users_path, user)) {
            const char* err = "type=error;text=user already registered";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Дописываем в users.txt
        if (users_append(users_path, user) != 0) {
            const char* err = "type=error;text=cannot register";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Подтверждаем регистрацию
        const char* ok = "type=info;text=registered";
        send_frame(c->sock, ok, (uint32_t)strlen(ok));
        log_info("Register user=%s", user);
        return 0;
    }

    // Логин: привязка user к текущему соединению
    if (strcmp(type, "login") == 0) {
        char user[MAX_USER];
        if (!kv_get(payload, "user", user, sizeof(user))) {
            const char* err = "type=error;text=missing user";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Запрещаем два онлайн-соединения на одного user
        int existing = find_by_user(clients, user);
        if (existing >= 0 && existing != client_idx) {
            const char* err = "type=error;text=user already online";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            log_info("Login rejected: user=%s already online", user);
            return 0;
        }

        // Проверяем, что user зарегистрирован
        if (!users_exists(users_path, user)) {
            const char* err = "type=error;text=user not registered";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            log_info("Login rejected: user=%s not registered", user);
            return 0;
        }

        // Сохраняем имя в слоте клиента
        strncpy(c->user, user, sizeof(c->user) - 1);
        c->user[sizeof(c->user) - 1] = '\0';

        // Отвечаем клиенту, что вход успешный
        char info[128];
        snprintf(info, sizeof(info), "type=info;text=login ok (%s)", c->user);
        send_frame(c->sock, info, (uint32_t)strlen(info));

        // Локальный вывод и лог
        printf("User logged in: %s\n", c->user);
        log_info("Login user=%s", c->user);
        return 0;
    }

    // Список онлайн-пользователей
    if (strcmp(type, "users") == 0) {
        char list[1024];
        list[0] = '\0';

        // Собираем список через запятую
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].used) continue;
            if (!clients[i].user[0]) continue;

            if (list[0]) strncat(list, ",", sizeof(list) - strlen(list) - 1);
            strncat(list, clients[i].user, sizeof(list) - strlen(list) - 1);
        }

        // Отправляем ответ (в зашифрованном виде)
        char out[1200];
        snprintf(out, sizeof(out), "type=users;list=%s", list);
        send_payload(c->sock, out);
        return 0;
    }

    // Список всех зарегистрированных пользователей (из файла)
    if (strcmp(type, "users_all") == 0) {
        FILE* f = fopen(users_path, "r");
        char list[2048];
        list[0] = '\0';

        // Читаем users.txt и собираем строку list=...
        if (f) {
            char line[128];
            while (fgets(line, sizeof(line), f)) {
                size_t n = strlen(line);
                while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) { line[n - 1] = '\0'; n--; }
                if (line[0] == '\0') continue;

                if (list[0]) strncat(list, ",", sizeof(list) - strlen(list) - 1);
                strncat(list, line, sizeof(list) - strlen(list) - 1);
            }
            fclose(f);
        }

        // Здесь ответ отправляется без send_payload (как в исходнике)
        char out[2300];
        snprintf(out, sizeof(out), "type=users_all;list=%s", list);
        send_frame(c->sock, out, (uint32_t)strlen(out));
        return 0;
    }

    // Список комнат (из памяти)
    if (strcmp(type, "rooms") == 0) {
        char list[1024];
        list[0] = '\0';

        // Собираем имена комнат через запятую
        for (int r = 0; r < MAX_ROOMS; r++) {
            if (!rooms[r].used) continue;
            if (!rooms[r].name[0]) continue;

            if (list[0]) strncat(list, ",", sizeof(list) - strlen(list) - 1);
            strncat(list, rooms[r].name, sizeof(list) - strlen(list) - 1);
        }

        // Отправляем список
        char out[1200];
        snprintf(out, sizeof(out), "type=rooms;list=%s", list);
        send_frame(c->sock, out, (uint32_t)strlen(out));
        return 0;
    }

    // История личных сообщений
    if (strcmp(type, "history_dm") == 0) {
        // Историю можно получать только после логина
        if (!c->user[0]) {
            const char* err = "type=error;text=not logged in";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Получаем peer и limit
        char peer[MAX_USER];
        char limit_s[32] = { 0 };
        int limit = 20;

        if (!kv_get(payload, "peer", peer, sizeof(peer))) {
            const char* err = "type=error;text=missing peer";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }
        if (kv_get(payload, "limit", limit_s, sizeof(limit_s))) limit = atoi(limit_s);

        // Составляем путь к файлу истории и отправляем последние строки
        char path[256];
        history_path_dm(path, sizeof(path), c->user, peer);
        history_send_last_lines(c->sock, "dm", path, limit);

        log_info("HISTORY dm user=%s peer=%s limit=%d", c->user, peer, limit);
        return 0;
    }

    // История комнаты
    if (strcmp(type, "history_room") == 0) {
        if (!c->user[0]) {
            const char* err = "type=error;text=not logged in";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Получаем room и limit
        char room[MAX_ROOM_NAME];
        char limit_s[32] = { 0 };
        int limit = 20;

        if (!kv_get(payload, "room", room, sizeof(room))) {
            const char* err = "type=error;text=missing room";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }
        if (kv_get(payload, "limit", limit_s, sizeof(limit_s))) limit = atoi(limit_s);

        // Составляем путь к файлу истории и отправляем последние строки
        char path[256];
        history_path_room(path, sizeof(path), room);
        history_send_last_lines(c->sock, "room", path, limit);

        log_info("HISTORY room user=%s room=%s limit=%d", c->user, room, limit);
        return 0;
    }

    // Личное сообщение
    if (strcmp(type, "msg") == 0) {
        if (!c->user[0]) {
            const char* err = "type=error;text=not logged in";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Достаём получателя, текст и timestamp
        char to[MAX_USER];
        char text_enc[1024];
        char text[1024];
        char ts_s[32] = { 0 };
        long long ts = 0;

        if (!kv_get(payload, "to", to, sizeof(to))) {
            const char* err = "type=error;text=missing to";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }
        if (!kv_get(payload, "text", text_enc, sizeof(text_enc))) {
            const char* err = "type=error;text=missing text";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Декодируем текст из percent-encoding
        if (url_decode(text_enc, text, sizeof(text)) != 0) {
            const char* err = "type=error;text=bad text encoding";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Парсим timestamp, если есть
        if (kv_get(payload, "ts", ts_s, sizeof(ts_s))) {
            ts = atoll(ts_s);
        }

        // Ищем адресата среди онлайн пользователей
        int dst = find_by_user(clients, to);
        if (dst < 0) {
            const char* err = "type=error;text=user offline";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            log_info("MSG from=%s to=%s failed: offline", c->user, to);
            return 0;
        }

        // Кодируем текст для сетевого протокола
        char text_net[1400];
        if (url_encode(text, text_net, sizeof(text_net)) != 0) {
            const char* err = "type=error;text=text too long";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Отправляем deliver адресату
        char out[1400];
        snprintf(out, sizeof(out), "type=deliver;from=%s;ts=%lld;text=%s", c->user, ts, text_net);
        if (send_frame(clients[dst].sock, out, (uint32_t)strlen(out)) != 0) {
            const char* err = "type=error;text=deliver failed";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Отправляем отправителю подтверждение доставки
        const char* ok = "type=info;text=delivered";
        send_frame(c->sock, ok, (uint32_t)strlen(ok));

        // Локальный вывод и логирование
        printf("MSG %s -> %s: %s\n", c->user, to, text);
        log_info("MSG from=%s to=%s text=%s", c->user, to, text);

        // Пишем строку истории в файл dm_*.txt
        char hpath[256];
        history_path_dm(hpath, sizeof(hpath), c->user, to);

        char hline[1400];
        snprintf(hline, sizeof(hline), "ts=%lld;from=%s;to=%s;text=%s", ts, c->user, to, text);
        history_append_line(hpath, hline);

        return 0;
    }

    // Создание комнаты
    if (strcmp(type, "room_create") == 0) {
        if (!c->user[0]) {
            const char* err = "type=error;text=not logged in";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Достаём имя комнаты
        char room[MAX_ROOM_NAME];
        if (!kv_get(payload, "room", room, sizeof(room))) {
            const char* err = "type=error;text=missing room";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Добавляем комнату в память
        int ar = add_room(rooms, room);
        if (ar == -2) {
            const char* err = "type=error;text=room already exists";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }
        if (ar < 0) {
            const char* err = "type=error;text=cannot create room";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // persist room name
        rooms_append(rooms_path, room);

        // Отвечаем подтверждением
        char info[128];
        snprintf(info, sizeof(info), "type=info;text=room created (%s)", room);
        send_frame(c->sock, info, (uint32_t)strlen(info));

        log_info("ROOM create user=%s room=%s", c->user, room);
        return 0;
    }

    // Вход в комнату
    if (strcmp(type, "room_join") == 0) {
        if (!c->user[0]) {
            const char* err = "type=error;text=not logged in";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Достаём имя комнаты
        char room[MAX_ROOM_NAME];
        if (!kv_get(payload, "room", room, sizeof(room))) {
            const char* err = "type=error;text=missing room";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Проверяем, что комната существует
        int ri = find_room(rooms, room);
        if (ri < 0) {
            const char* err = "type=error;text=no such room";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Добавляем пользователя в список участников
        if (room_add_member(&rooms[ri], client_idx) != 0) {
            const char* err = "type=error;text=room is full";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Подтверждаем вход
        char info[128];
        snprintf(info, sizeof(info), "type=info;text=joined room (%s)", room);
        send_frame(c->sock, info, (uint32_t)strlen(info));

        log_info("ROOM join user=%s room=%s", c->user, room);
        return 0;
    }

    // Выход из комнаты
    if (strcmp(type, "room_leave") == 0) {
        if (!c->user[0]) {
            const char* err = "type=error;text=not logged in";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Достаём имя комнаты
        char room[MAX_ROOM_NAME];
        if (!kv_get(payload, "room", room, sizeof(room))) {
            const char* err = "type=error;text=missing room";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Проверяем, что комната существует
        int ri = find_room(rooms, room);
        if (ri < 0) {
            const char* err = "type=error;text=no such room";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Удаляем пользователя из участников
        room_remove_member(&rooms[ri], client_idx);

        // Подтверждаем выход
        char info[128];
        snprintf(info, sizeof(info), "type=info;text=left room (%s)", room);
        send_frame(c->sock, info, (uint32_t)strlen(info));

        log_info("ROOM leave user=%s room=%s", c->user, room);
        return 0;
    }

    // Сообщение в комнату
    if (strcmp(type, "room_msg") == 0) {
        if (!c->user[0]) {
            const char* err = "type=error;text=not logged in";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Достаём параметры: room, text, ts
        char room[MAX_ROOM_NAME];
        char text_enc[1024];
        char text[1024];
        char ts_s[32] = { 0 };
        long long ts = 0;

        if (!kv_get(payload, "room", room, sizeof(room))) {
            const char* err = "type=error;text=missing room";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }
        if (!kv_get(payload, "text", text_enc, sizeof(text_enc))) {
            const char* err = "type=error;text=missing text";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Декодируем текст
        if (url_decode(text_enc, text, sizeof(text)) != 0) {
            const char* err = "type=error;text=bad text encoding";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Парсим ts
        if (kv_get(payload, "ts", ts_s, sizeof(ts_s))) {
            ts = atoll(ts_s);
        }

        // Проверяем комнату
        int ri = find_room(rooms, room);
        if (ri < 0) {
            const char* err = "type=error;text=no such room";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Пользователь должен быть участником комнаты
        if (!room_has_member(&rooms[ri], client_idx)) {
            const char* err = "type=error;text=not in room";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Кодируем текст для доставки участникам
        char text_net[1400];
        if (url_encode(text, text_net, sizeof(text_net)) != 0) {
            const char* err = "type=error;text=text too long";
            send_frame(c->sock, err, (uint32_t)strlen(err));
            return 0;
        }

        // Формируем сообщение доставки в комнату
        char out[MAX_PAYLOAD];
        snprintf(out, sizeof(out), "type=room_deliver;room=%s;from=%s;ts=%lld;text=%s",
            room, c->user, ts, text_net);

        // Рассылаем всем участникам комнаты
        room_broadcast(&rooms[ri], clients, out, -1);

        // Подтверждаем отправителю
        const char* ok = "type=info;text=room message sent";
        send_frame(c->sock, ok, (uint32_t)strlen(ok));

        // Логируем и пишем историю комнаты
        log_info("ROOM msg from=%s room=%s text=%s", c->user, room, text);

        char hpath[256];
        history_path_room(hpath, sizeof(hpath), room);

        char hline[1400];
        snprintf(hline, sizeof(hline), "ts=%lld;from=%s;room=%s;text=%s", ts, c->user, room, text);
        history_append_line(hpath, hline);

        return 0;
    }

    // Неизвестная команда
    const char* info = "type=info;text=unknown command";
    send_frame(c->sock, info, (uint32_t)strlen(info));
    return 0;
}

// Точка входа сервера: настройка сети, загрузка комнат, цикл select(), завершение по сигналу
int main(int argc, char** argv) {
    // Аргументы: порт, путь к логу, опционально путь к users.txt
    if (argc != 3 && argc != 4) { usage(argv[0]); return 1; }

    // Парсим аргументы запуска
    int port = atoi(argv[1]);
    const char* log_path = argv[2];
    const char* users_path = (argc == 4) ? argv[3] : "server/data/users.txt";
    const char* rooms_path = "server/data/rooms.txt";

    // Проверяем порт
    if (port <= 0 || port > 65535) { fprintf(stderr, "Bad port\n"); return 1; }

    // Инициализация сетевого слоя
    if (net_init() != 0) {
        fprintf(stderr, "net_init failed, err=%d\n", net_last_error());
        return 1;
    }

    // Инициализация криптографии (ключ берётся из MSG_KEY)
    crypto_init();

    // Открываем лог
    if (log_init(log_path) != 0) {
        fprintf(stderr, "Cannot open log file: %s\n", log_path);
        net_cleanup();
        return 1;
    }
    log_info("Server start (port=%d)", port);

    // Убеждаемся, что users-файл существует
    if (users_ensure_file(users_path) != 0) {
        fprintf(stderr, "Cannot open users file: %s\n", users_path);
        log_info("Cannot open users file: %s", users_path);
        log_close();
        net_cleanup();
        return 1;
    }
    log_info("Users file: %s", users_path);

    // Подписываемся на сигналы завершения
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    // Создаём listening socket
    sock_t ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls == SOCK_INVALID) {
        fprintf(stderr, "socket failed, err=%d\n", net_last_error());
        net_cleanup();
        return 1;
    }

    // Разрешаем переиспользование адреса при быстром перезапуске сервера
    int opt = 1;
#ifdef _WIN32
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    // Биндимся на INADDR_ANY:port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);

    if (bind(ls, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "bind failed, err=%d\n", net_last_error());
        sock_close(ls);
        net_cleanup();
        return 1;
    }

    // Начинаем слушать входящие соединения
    if (listen(ls, 16) != 0) {
        fprintf(stderr, "listen failed, err=%d\n", net_last_error());
        sock_close(ls);
        net_cleanup();
        return 1;
    }

    printf("Server listening on port %d...\n", port);

    // Массив клиентов (фиксированный размер)
    Client clients[MAX_CLIENTS];
    memset(clients, 0, sizeof(clients));
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].sock = SOCK_INVALID;

    // Массив комнат (фиксированный размер)
    Room rooms[MAX_ROOMS];
    memset(rooms, 0, sizeof(rooms));

    // Load rooms from file (persistent rooms)
    if (rooms_load(rooms, rooms_path) == 0) {
        log_info("Rooms loaded from %s", rooms_path);
    }
    else {
        log_info("Rooms file not loaded (will create on demand): %s", rooms_path);
        // ensure file exists
        FILE* rf = fopen(rooms_path, "a");
        if (rf) fclose(rf);
    }

    // Главный цикл: select() по listening socket и всем клиентским сокетам
    while (!g_stop) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(ls, &rfds);

        // Вычисляем максимальный дескриптор для select()
        sock_t maxfd = ls;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].used) {
                FD_SET(clients[i].sock, &rfds);
                if (clients[i].sock > maxfd) maxfd = clients[i].sock;
            }
        }

        // Блокируемся в select() до активности
        int rc = select((int)(maxfd + 1), &rfds, NULL, NULL, NULL);
        if (rc < 0) {
#ifndef _WIN32
            // EINTR возникает при сигнале, в этом случае просто продолжаем
            if (net_last_error() == EINTR) continue;
#endif
            fprintf(stderr, "select failed, err=%d\n", net_last_error());
            continue;
        }

        // Новое входящее соединение
        if (FD_ISSET(ls, &rfds)) {
            struct sockaddr_in caddr;
#ifdef _WIN32
            int clen = sizeof(caddr);
#else
            socklen_t clen = sizeof(caddr);
#endif
            sock_t cs = accept(ls, (struct sockaddr*)&caddr, &clen);
            if (cs != SOCK_INVALID) {
                int idx = add_client(clients, cs);
                if (idx < 0) {
                    const char* err = "type=error;text=server full";
                    send_frame(cs, err, (uint32_t)strlen(err));
                    sock_close(cs);
                }
                else {
                    printf("Client connected (slot=%d).\n", idx);
                    log_info("Client connected (slot=%d)", idx);
                }
            }
        }

        // Обработка данных от клиентов
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].used) continue;
            if (!FD_ISSET(clients[i].sock, &rfds)) continue;

            // Принимаем один протокольный payload
            char payload[MAX_PAYLOAD + 1];
            uint32_t n = 0;

            int rr = recv_payload(clients[i].sock, payload, MAX_PAYLOAD, &n);
            if (rr != 0) {
                // Клиент отключился или ошибка
                printf("Client disconnected (slot=%d, user=%s)\n", i,
                    clients[i].user[0] ? clients[i].user : "<none>");
                log_info("Client disconnected (slot=%d, user=%s)", i,
                    clients[i].user[0] ? clients[i].user : "<none>");

                // Убираем клиента из всех комнат
                for (int r = 0; r < MAX_ROOMS; r++) {
                    if (rooms[r].used) room_remove_member(&rooms[r], i);
                }

                // Освобождаем слот клиента
                remove_client(clients, i);
                continue;
            }

            // Гарантируем нуль-терминацию и передаём на обработчик команд
            payload[n] = '\0';
            handle_frame(clients, rooms, users_path, rooms_path, i, &clients[i], payload);
        }
    }

    // При остановке сервера уведомляем клиентов
    {
        const char* msg = "type=info;text=server shutting down";
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].used) continue;
            // ignore send errors: clients may be already disconnected
            send_frame(clients[i].sock, msg, (uint32_t)strlen(msg));
        }
        log_info("Shutdown: notified clients");
    }

    // Закрываем все соединения
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].used) remove_client(clients, i);
    }

    // Закрываем listening socket и чистим ресурсы
    sock_close(ls);
    log_info("Server stop");
    log_close();
    net_cleanup();
    crypto_cleanup();
    return 0;
}