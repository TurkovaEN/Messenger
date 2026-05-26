#pragma once

// Главное окно Qt-клиента: список чатов, история, поле ввода и кнопки управления
#include <QMainWindow>
#include <QMap>
#include <QStringList>
#include <QSet>
#include <QTimer>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTextEdit;
class QLineEdit;
class NetClient;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    // Создаёт окно и привязывает его к NetClient (сигналы/слоты)
    explicit MainWindow(NetClient* net, QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    // UI обработчики
    void onSendClicked();
    void onRefreshClicked();
    void onCreateRoomClicked();
    void onJoinRoomClicked();
    void onChatSelected(QListWidgetItem* item);

    // Сетевые события
    void onNetDisconnected();
    void onNetError(const QString& msg);
    void onNetMessage(const QString& msg);

    // События протокола: новое сообщение в конкретный чат и история
    void onChatMessage(const QString& chatKey, const QString& line, qint64 ts);
    void onHistoryItem(const QString& chatKey, const QString& line);
    void onHistoryEnd(const QString& chatKey);

private:
    // Блокирует/разблокирует элементы интерфейса при потере соединения
    void setUiEnabled(bool connected);

    // Полная перерисовка текущего чата из m_chatLog
    void redrawCurrentChat();

    // Перерисовка списка пользователей (@user) на основе online/all
    void redrawUsers();

private:
    // Сетевой слой (владелец — main.cpp)
    NetClient* m_net;

    // UI
    QListWidget* m_chats = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_createRoomBtn = nullptr;
    QPushButton* m_joinRoomBtn = nullptr;
    QTextEdit* m_log = nullptr;
    QLineEdit* m_text = nullptr;
    QPushButton* m_sendBtn = nullptr;
    QTimer* m_refreshTimer = nullptr;

    // state
    // m_currentChat хранится как "@bob" или "#room1"
    QString m_currentChat;
    // Кэш истории чатов (для быстрого отображения)
    QMap<QString, QStringList> m_chatLog;
    // Набор комнат, в которые клиент уже вступил (имя без '#')
    QSet<QString> m_joinedRooms;
    // Флаг: сейчас идёт загрузка истории (чтобы не мешать NEW-сепаратору)
    bool m_loadingHistory = false;
    // Время открытия текущего чата (для отметки "NEW")
    qint64 m_chatOpenedTs = 0;
    // Был ли уже показан разделитель NEW для текущего открытия чата
    bool m_newSeparatorShown = false;
    // Нужно ли показать NEW после подгрузки истории (если были непрочитанные)
    bool m_showNewAfterHistory = false;

    // users
    // Все зарегистрированные пользователи
    QSet<QString> m_allUsers;
    // Онлайн пользователи
    QSet<QString> m_onlineUsers;
    // Счётчик непрочитанных по chatKey
    QMap<QString, int> m_unread;
};