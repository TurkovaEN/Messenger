// Реализация сетевого клиента Qt: фрейминг, AES-шифрование payload,
// распознавание протокольных команд и выдача высокоуровневых сигналов UI
#include "NetClient.h"

#include <QtEndian>
#include <QDateTime>

// Подключаем C-реализацию криптографии из common (AES-256-CBC + base64)
extern "C" {
#include "../../common/crypto.h"
}

// Упрощённый kv-парсер для QString: извлекает значение по ключу из "a=1;b=2"
static QString kvGet(const QString& s, const QString& key) {
    int pos = 0;
    while (pos < s.size()) {
        int eq = s.indexOf('=', pos);
        if (eq < 0) break;

        QString k = s.mid(pos, eq - pos);
        int semi = s.indexOf(';', eq + 1);
        QString v = (semi < 0) ? s.mid(eq + 1) : s.mid(eq + 1, semi - (eq + 1));

        if (k == key) return v;
        if (semi < 0) break;
        pos = semi + 1;
    }
    return {};
}

// Приводит строку истории (ts=...;from=...;text=...) к виду "name: text\nDD.MM HH:mm"
static QString formatHistoryLine(const QString& raw, const QString& selfUser) {
    QString from = kvGet(raw, "from");
    QString text = kvGet(raw, "text");
    QString tsStr = kvGet(raw, "ts");
    qint64 ts = tsStr.toLongLong();

    // Форматируем время в локальную дату/время
    QString timeStr = ts > 0
        ? QDateTime::fromSecsSinceEpoch(ts).toLocalTime().toString("dd.MM HH:mm")
        : QString();

    // Если строка не похожа на kv-формат — возвращаем как есть
    if (from.isEmpty() && text.isEmpty()) return raw;

    // Для собственных сообщений показываем "me"
    QString name = from;
    if (!selfUser.isEmpty() && from == selfUser) name = "me";

    // Собираем красивую строку
    QString line = QString("%1: %2").arg(name, text);
    if (!timeStr.isEmpty()) line += "\n" + timeStr;
    return line;
}

// URL-encoding (percent) для QByteArray (используется для text=... в протоколе)
static QByteArray urlEncode(const QByteArray& in) {
    static const char* H = "0123456789ABCDEF";
    QByteArray out;
    out.reserve(in.size() * 3);

    for (unsigned char c : in) {
        bool safe =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~' || c == ' ';

        if (safe) out.append((char)c);
        else {
            out.append('%');
            out.append(H[(c >> 4) & 0xF]);
            out.append(H[c & 0xF]);
        }
    }
    return out;
}

// Преобразует hex символ в значение 0..15, либо -1
static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

// URL-decoding для QByteArray
static QByteArray urlDecode(const QByteArray& in) {
    QByteArray out;
    out.reserve(in.size());

    for (int i = 0; i < in.size(); i++) {
        char c = in[i];

        // Распознаём %XX
        if (c == '%' && i + 2 < in.size()) {
            int hi = hexVal(in[i + 1]);
            int lo = hexVal(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.append((char)((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.append(c);
    }
    return out;
}

NetClient::NetClient(QObject* parent)
    : QObject(parent)
{
    // Инициализируем crypto (ключ из MSG_KEY или demo)
    crypto_init();

    // Подключаем события сокета к слотам NetClient
    connect(&m_sock, &QTcpSocket::connected, this, &NetClient::onConnected);
    connect(&m_sock, &QTcpSocket::readyRead, this, &NetClient::onReadyRead);
    connect(&m_sock, &QTcpSocket::disconnected, this, &NetClient::onDisconnected);
    connect(&m_sock, &QTcpSocket::errorOccurred, this, &NetClient::onError);
}

// Запускает подключение, сохраняет параметры пользователя и сбрасывает состояние
void NetClient::connectTo(const QString& host, quint16 port, const QString& user, bool doRegister) {
    m_user = user;
    m_doRegister = doRegister;
    m_loggedIn = false;

    // Очищаем входной буфер и разрываем предыдущие попытки
    m_buf.clear();
    m_sock.abort();

    // Подключаемся к серверу
    m_sock.connectToHost(host, port);
}

// Запрос списка онлайн пользователей
void NetClient::requestUsers() {
    sendFrame("type=users");
}

// Запрос списка всех зарегистрированных пользователей
void NetClient::requestUsersAll() {
    sendFrame("type=users_all");
}

// Запрос списка комнат
void NetClient::requestRooms() {
    sendFrame("type=rooms");
}

// Создание комнаты на сервере
void NetClient::createRoom(const QString& room) {
    QByteArray payload = "type=room_create;room=" + room.toUtf8();
    sendFrame(payload);
}

// Вступление в комнату
void NetClient::joinRoom(const QString& room) {
    QByteArray payload = "type=room_join;room=" + room.toUtf8();
    sendFrame(payload);
}

// Отправка личного сообщения
void NetClient::sendDm(const QString& to, const QString& text) {
    // Кодируем текст, чтобы безопасно передать через key=value
    QByteArray textEnc = urlEncode(text.toUtf8());
    qint64 ts = QDateTime::currentSecsSinceEpoch();

    // Формируем payload протокола
    QByteArray payload = "type=msg;to=" + to.toUtf8()
        + ";ts=" + QByteArray::number(ts)
        + ";text=" + textEnc;

    sendFrame(payload);
}

// Отправка сообщения в комнату
void NetClient::sendRoom(const QString& room, const QString& text) {
    QByteArray textEnc = urlEncode(text.toUtf8());
    qint64 ts = QDateTime::currentSecsSinceEpoch();

    QByteArray payload = "type=room_msg;room=" + room.toUtf8()
        + ";ts=" + QByteArray::number(ts)
        + ";text=" + textEnc;

    sendFrame(payload);
}

// Запрос истории DM: сохраняем chatKey, чтобы правильно маршрутизировать history_item
void NetClient::requestHistoryDm(const QString& peer, int limit) {
    m_historyChatKey = "@" + peer;
    QByteArray payload = "type=history_dm;peer=" + peer.toUtf8()
        + ";limit=" + QByteArray::number(limit);
    sendFrame(payload);
}

// Запрос истории комнаты: сохраняем chatKey, чтобы правильно маршрутизировать history_item
void NetClient::requestHistoryRoom(const QString& room, int limit) {
    m_historyChatKey = "#" + room;
    QByteArray payload = "type=history_room;room=" + room.toUtf8()
        + ";limit=" + QByteArray::number(limit);
    sendFrame(payload);
}

// Отправляет один frame: шифрует payloadPlain и добавляет 4-байтный префикс длины
void NetClient::sendFrame(const QByteArray& payloadPlain) {
    // encrypt payloadPlain -> send as type=enc;data=...
    char b64[8192];

    // Шифруем plaintext через common/crypto (AES-256-CBC)
    if (crypto_encrypt_b64((const unsigned char*)payloadPlain.constData(),
                           (size_t)payloadPlain.size(),
                           b64, sizeof(b64)) != 0) {
        emit error("crypto_encrypt_b64 failed");
        return;
    }

    // Упаковываем во внешний контейнер протокола
    QByteArray payloadEnc = "type=enc;data=" + QByteArray(b64);

    // Готовим длину кадра в big-endian
    quint32 len = (quint32)payloadEnc.size();
    quint32 netLen = qToBigEndian(len);

    // Собираем frame: 4 bytes len + payload
    QByteArray frame;
    frame.resize(4);
    memcpy(frame.data(), &netLen, 4);
    frame.append(payloadEnc);

    // Отправляем в сокет
    m_sock.write(frame);
}

// Событие QTcpSocket::connected: после TCP connect отправляем register (опционально) и login
void NetClient::onConnected() {
    // Если пользователь выбрал Register — сначала регистрируемся
    if (m_doRegister) {
        QByteArray reg = "type=register;user=" + m_user.toUtf8();
        sendFrame(reg);
    }

    // Затем отправляем login
    QByteArray login = "type=login;user=" + m_user.toUtf8();
    sendFrame(login);
}

// Событие readyRead: читаем данные, собираем кадры и обрабатываем каждый payload
void NetClient::onReadyRead() {
    // Дочитываем все доступные байты в буфер
    m_buf.append(m_sock.readAll());

    // В цикле извлекаем кадры: сначала длина, затем payload
    while (true) {
        if (m_buf.size() < 4) return;

        // Считываем длину кадра
        quint32 netLen = 0;
        memcpy(&netLen, m_buf.constData(), 4);
        quint32 len = qFromBigEndian(netLen);

        // Если payload ещё не полностью пришёл — ждём следующего readyRead
        if (m_buf.size() < 4 + (int)len) return;

        // Извлекаем payload и удаляем кадр из буфера
        QByteArray payload = m_buf.mid(4, (int)len);
        m_buf.remove(0, 4 + (int)len);

        // Обрабатываем payload
        processFrame(payload);
    }
}

// Обрабатывает один протокольный payload: расшифровка enc и маршрутизация по type
void NetClient::processFrame(const QByteArray& payloadBytes) {
    // Декодируем payload как UTF-8 строку
    QString payload = QString::fromUtf8(payloadBytes);

    // decrypt if needed
    const QString outerType = kvGet(payload, "type");
    if (outerType == "enc") {
        const QString data = kvGet(payload, "data");
        if (data.isEmpty()) {
            emit message("[error] enc frame without data");
            return;
        }

        // Дешифруем base64(IV+ciphertext) обратно в plaintext
        QByteArray b64 = data.toUtf8();
        QByteArray plainBuf;
        plainBuf.resize(4096);

        size_t plainLen = 0;
        if (crypto_decrypt_b64(b64.constData(),
                               (unsigned char*)plainBuf.data(),
                               (size_t)plainBuf.size(),
                               &plainLen) != 0) {
            emit message("[error] decrypt failed");
            return;
        }

        // Перекодируем расшифрованный payload в QString
        payload = QString::fromUtf8(plainBuf.constData(), (int)plainLen);
    }

    // Достаём внутренний type
    const QString type = kvGet(payload, "type");

    // Личное сообщение от сервера (deliver)
    if (type == "deliver") {
        QString from = kvGet(payload, "from");

        // Если это эхо собственного сообщения — не показываем (мы уже добавили локально)
        if (from == m_user) {
            return; // already shown locally
        }

        // Парсим timestamp для UI (NEW-separator)
        QString tsStr = kvGet(payload, "ts");
        qint64 ts = tsStr.toLongLong();
        QString timeStr = ts > 0
            ? QDateTime::fromSecsSinceEpoch(ts).toLocalTime().toString("dd.MM HH:mm")
            : QString();

        // Декодируем текст из url-encoded вида
        QByteArray textEnc = kvGet(payload, "text").toUtf8();
        QByteArray text = urlDecode(textEnc);

        // DM чатKey имеет формат "@from"
        const QString chatKey = "@" + from;

        // Формируем строку для отображения
        QString line = QString("%1: %2").arg(from, QString::fromUtf8(text));
        if (!timeStr.isEmpty()) line += "\n" + timeStr;

        emit messageForChat(chatKey, line, ts);
        return;
    }

    // Сообщение в комнате от сервера (room_deliver)
    if (type == "room_deliver") {
        QString room = kvGet(payload, "room");
        QString from = kvGet(payload, "from");

        // Парсим timestamp
        QString tsStr = kvGet(payload, "ts");
        qint64 ts = tsStr.toLongLong();
        QString timeStr = ts > 0
            ? QDateTime::fromSecsSinceEpoch(ts).toLocalTime().toString("dd.MM HH:mm")
            : QString();

        // Декодируем текст
        QByteArray textEnc = kvGet(payload, "text").toUtf8();
        QByteArray text = urlDecode(textEnc);

        // Комнатный chatKey имеет формат "#room"
        const QString chatKey = "#" + room;

        // Формируем строку UI
        QString line = QString("%1: %2").arg(from, QString::fromUtf8(text));
        if (!timeStr.isEmpty()) line += "\n" + timeStr;

        emit messageForChat(chatKey, line, ts);
        return;
    }

    // Список онлайн пользователей
    if (type == "users") {
        QString list = kvGet(payload, "list");
        QStringList items = list.split(",", Qt::SkipEmptyParts);
        for (QString& s : items) s = s.trimmed();
        emit usersList(items);
        return;
    }

    // Список всех пользователей
    if (type == "users_all") {
        QString list = kvGet(payload, "list");
        QStringList items = list.split(",", Qt::SkipEmptyParts);
        for (QString& s : items) s = s.trimmed();
        emit usersAllList(items);
        return;
    }

    // Список комнат
    if (type == "rooms") {
        QString list = kvGet(payload, "list");
        QStringList items = list.split(",", Qt::SkipEmptyParts);
        for (QString& s : items) s = s.trimmed();
        emit roomsList(items);
        return;
    }

    // Элемент истории (история приходит без привязки к конкретному чату, поэтому используем m_historyChatKey)
    if (type == "history_item") {
        QString chat = kvGet(payload, "chat"); // "dm" or "room"
        QByteArray lineEnc = kvGet(payload, "line").toUtf8();
        QByteArray line = urlDecode(lineEnc);

        // We route history to the last requested chatKey
        QString key = m_historyChatKey;
        if (key.isEmpty()) {
            // fallback: show as debug
            emit message(QString("[history %1] %2").arg(chat, QString::fromUtf8(line)));
            return;
        }

        // Приводим строку истории к формату UI
        QString rawLine = QString::fromUtf8(line);
        QString pretty = formatHistoryLine(rawLine, m_user);
        emit historyItem(key, pretty);
        return;
    }

    // Конец истории
    if (type == "history_end") {
        QString key = m_historyChatKey;
        if (!key.isEmpty()) {
            emit historyEnd(key);
            m_historyChatKey.clear();
        }
        return;
    }

    // info/error сообщения от сервера
    if (type == "info" || type == "error") {
        QString text = kvGet(payload, "text");

        // Скрываем внутренний ack "delivered"
        if (type == "info" && text == "delivered") {
            return;
        }

        // connected() эмитим только после успешного login ok
        if (type == "info") {
            if (!m_loggedIn && text.startsWith("login ok")) {
                m_loggedIn = true;
                emit connected();
            }
        }

        // Прокидываем сообщение в UI
        emit message(QString("[%1] %2").arg(type, text));
        return;
    }

    // Неизвестные сообщения показываем как raw
    emit message("[raw] " + payload);
}

// Событие disconnect: сбрасываем флаг логина и уведомляем UI
void NetClient::onDisconnected() {
    m_loggedIn = false;
    emit disconnected();
}

// Событие ошибки сокета: отправляем строку ошибки в UI
void NetClient::onError(QAbstractSocket::SocketError) {
    emit error(m_sock.errorString());
}