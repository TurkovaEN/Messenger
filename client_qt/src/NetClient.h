#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QStringList>

class NetClient : public QObject {
    Q_OBJECT
public:
    explicit NetClient(QObject* parent = nullptr);

    void connectTo(const QString& host, quint16 port, const QString& user, bool doRegister);

    void requestUsers();
    void requestRooms();

    void createRoom(const QString& room);
    void joinRoom(const QString& room);

    void sendDm(const QString& to, const QString& text);
    void sendRoom(const QString& room, const QString& text);

signals:
    // emitted after "login ok"
    void connected();
    void disconnected();
    void error(const QString& msg);
    void message(const QString& msg);

    void usersList(const QStringList& users);
    void roomsList(const QStringList& rooms);

private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError);

private:
    void sendFrame(const QByteArray& payload);
    void processFrame(const QByteArray& payload);

    QTcpSocket m_sock;
    QByteArray m_buf;

    QString m_user;
    bool m_doRegister = false;
    bool m_loggedIn = false;
};
