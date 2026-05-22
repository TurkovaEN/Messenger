#include "NetClient.h"

#include <QtEndian>

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

static int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static QByteArray urlDecode(const QByteArray& in) {
    QByteArray out;
    out.reserve(in.size());
    for (int i = 0; i < in.size(); i++) {
        char c = in[i];
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
    connect(&m_sock, &QTcpSocket::connected, this, &NetClient::onConnected);
    connect(&m_sock, &QTcpSocket::readyRead, this, &NetClient::onReadyRead);
    connect(&m_sock, &QTcpSocket::disconnected, this, &NetClient::onDisconnected);
    connect(&m_sock, &QTcpSocket::errorOccurred, this, &NetClient::onError);
}

void NetClient::connectTo(const QString& host, quint16 port, const QString& user, bool doRegister) {
    m_user = user;
    m_doRegister = doRegister;
    m_loggedIn = false;
    m_buf.clear();
    m_sock.abort();
    m_sock.connectToHost(host, port);
}

void NetClient::requestUsers() {
    sendFrame("type=users");
}

void NetClient::requestRooms() {
    sendFrame("type=rooms");
}

void NetClient::createRoom(const QString& room) {
    QByteArray payload = "type=room_create;room=" + room.toUtf8();
    sendFrame(payload);
}

void NetClient::joinRoom(const QString& room) {
    QByteArray payload = "type=room_join;room=" + room.toUtf8();
    sendFrame(payload);
}

void NetClient::sendDm(const QString& to, const QString& text) {
    QByteArray textEnc = urlEncode(text.toUtf8());
    QByteArray payload = "type=msg;to=" + to.toUtf8() + ";text=" + textEnc;
    sendFrame(payload);
}

void NetClient::sendRoom(const QString& room, const QString& text) {
    QByteArray textEnc = urlEncode(text.toUtf8());
    QByteArray payload = "type=room_msg;room=" + room.toUtf8() + ";text=" + textEnc;
    sendFrame(payload);
}

void NetClient::requestHistoryDm(const QString& peer, int limit) {
    m_historyChatKey = "@" + peer;
    QByteArray payload = "type=history_dm;peer=" + peer.toUtf8() + ";limit=" + QByteArray::number(limit);
    sendFrame(payload);
}

void NetClient::requestHistoryRoom(const QString& room, int limit) {
    m_historyChatKey = "#" + room;
    QByteArray payload = "type=history_room;room=" + room.toUtf8() + ";limit=" + QByteArray::number(limit);
    sendFrame(payload);
}

void NetClient::sendFrame(const QByteArray& payload) {
    quint32 len = (quint32)payload.size();
    quint32 netLen = qToBigEndian(len);

    QByteArray frame;
    frame.resize(4);
    memcpy(frame.data(), &netLen, 4);
    frame.append(payload);

    m_sock.write(frame);
}

void NetClient::onConnected() {
    if (m_doRegister) {
        QByteArray reg = "type=register;user=" + m_user.toUtf8();
        sendFrame(reg);
    }
    QByteArray login = "type=login;user=" + m_user.toUtf8();
    sendFrame(login);
}

void NetClient::onReadyRead() {
    m_buf.append(m_sock.readAll());

    while (true) {
        if (m_buf.size() < 4) return;

        quint32 netLen = 0;
        memcpy(&netLen, m_buf.constData(), 4);
        quint32 len = qFromBigEndian(netLen);

        if (m_buf.size() < 4 + (int)len) return;

        QByteArray payload = m_buf.mid(4, (int)len);
        m_buf.remove(0, 4 + (int)len);
        processFrame(payload);
    }
}

void NetClient::processFrame(const QByteArray& payloadBytes) {
    QString payload = QString::fromUtf8(payloadBytes);
    const QString type = kvGet(payload, "type");

    if (type == "deliver") {
        QString from = kvGet(payload, "from");
        QByteArray textEnc = kvGet(payload, "text").toUtf8();
        QByteArray text = urlDecode(textEnc);

        const QString chatKey = "@" + from;
        const QString line = QString("%1: %2").arg(from, QString::fromUtf8(text));
        emit messageForChat(chatKey, line);
        return;
    }

        if (type == "room_deliver") {
        QString room = kvGet(payload, "room");
        QString from = kvGet(payload, "from");
        QByteArray textEnc = kvGet(payload, "text").toUtf8();
        QByteArray text = urlDecode(textEnc);

        const QString chatKey = "#" + room;
        const QString line = QString("%1: %2").arg(from, QString::fromUtf8(text));
        emit messageForChat(chatKey, line);
        return;
    }

    if (type == "users") {
        QString list = kvGet(payload, "list");
        QStringList items = list.split(",", Qt::SkipEmptyParts);
        for (QString& s : items) s = s.trimmed();
        emit usersList(items);
        emit message(QString("[users] %1").arg(list));
        return;
    }

    if (type == "rooms") {
        QString list = kvGet(payload, "list");
        QStringList items = list.split(",", Qt::SkipEmptyParts);
        for (QString& s : items) s = s.trimmed();
        emit roomsList(items);
        emit message(QString("[rooms] %1").arg(list));
        return;
    }

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

        emit historyItem(key, QString::fromUtf8(line));
        return;
    }

    if (type == "history_end") {
        QString key = m_historyChatKey;
        if (!key.isEmpty()) {
            emit historyEnd(key);
            m_historyChatKey.clear();
        }
        return;
    }

    if (type == "info" || type == "error") {
        QString text = kvGet(payload, "text");
 if (type == "info" && text == "delivered") {
        return;
    }
        if (type == "info") {
            if (!m_loggedIn && text.startsWith("login ok")) {
                m_loggedIn = true;
                emit connected();
            }
        }

        emit message(QString("[%1] %2").arg(type, text));
        return;
    }

    emit message("[raw] " + payload);
}

void NetClient::onDisconnected() {
    m_loggedIn = false;
    emit disconnected();
}

void NetClient::onError(QAbstractSocket::SocketError) {
    emit error(m_sock.errorString());
}
