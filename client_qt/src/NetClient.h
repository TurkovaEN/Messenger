#pragma once

// Сетевой клиент Qt: подключение по TCP, упаковка/распаковка фреймов,
// шифрование payload через common/crypto и преобразование протокольных сообщений в сигналы
#include <QObject>
#include <QTcpSocket>
#include <QStringList>

class NetClient : public QObject {
    Q_OBJECT

public:
    // Создаёт объект и подключает сигналы QTcpSocket к своим слотам
    explicit NetClient(QObject* parent = nullptr);

    // Подключиться к серверу и сохранить параметры пользователя
    void connectTo(const QString& host, quint16 port, const QString& user, bool doRegister);

    // Запросы к серверу (оборачивают sendFrame)
    void requestUsers();
    void requestUsersAll();
    void requestRooms();

    // Команды комнат
    void createRoom(const QString& room);
    void joinRoom(const QString& room);

    // Отправка сообщений
    void sendDm(const QString& to, const QString& text);
    void sendRoom(const QString& room, const QString& text);

    // Запрос истории
    void requestHistoryDm(const QString& peer, int limit);
    void requestHistoryRoom(const QString& room, int limit);

signals:
    // Сигналы состояния соединения
    void connected(); // emitted after "login ok"
    void disconnected();
    void error(const QString& msg);

    // Сервисные сообщения (info/error/raw)
    void message(const QString& msg);

    // Сообщение для конкретного чата: chatKey="@user" или "#room"
    void messageForChat(const QString& chatKey, const QString& line, qint64 ts);

    // Списки пользователей/комнат
    void usersList(const QStringList& users);
    void roomsList(const QStringList& rooms);
    void usersAllList(const QStringList& users);

    // История: элементы и конец
    void historyItem(const QString& chatKey, const QString& line);
    void historyEnd(const QString& chatKey);

private slots:
    // Слоты событий сокета
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError);

private:
    // Отправка одного протокольного payload (внутри делает шифрование и фрейминг)
    void sendFrame(const QByteArray& payload);

    // Обработка принятого payload (включая расшифровку type=enc)
    void processFrame(const QByteArray& payload);

    // TCP сокет Qt
    QTcpSocket m_sock;

    // Входной буфер для сборки фреймов (может приходить частями)
    QByteArray m_buf;

    // Параметры пользователя
    QString m_user;
    bool m_doRegister = false;
    bool m_loggedIn = false;

    // which chat history is currently being loaded ("@bob" or "#room")
    QString m_historyChatKey;
};