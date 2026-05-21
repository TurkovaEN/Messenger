#pragma once

#include <QObject>
#include <QTcpSocket>

class NetClient : public QObject {
    Q_OBJECT
public:
    explicit NetClient(QObject* parent = nullptr);

    void connectTo(const QString& host, quint16 port, const QString& user);
    void sendDm(const QString& to, const QString& text);

signals:
    void connected();
    void disconnected();
    void error(const QString& msg);
    void message(const QString& msg);

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
};
