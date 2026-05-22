#pragma once

#include <QMainWindow>
#include <QMap>
#include <QStringList>
#include <QSet>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTextEdit;
class QLineEdit;

class NetClient;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(NetClient* net, QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onSendClicked();

    void onRefreshClicked();
    void onCreateRoomClicked();
    void onJoinRoomClicked();
    void onChatSelected(QListWidgetItem* item);

    void onNetDisconnected();
    void onNetError(const QString& msg);
    void onNetMessage(const QString& msg);

    void onChatMessage(const QString& chatKey, const QString& line);

    void onHistoryItem(const QString& chatKey, const QString& line);
    void onHistoryEnd(const QString& chatKey);

private:
    void setUiEnabled(bool connected);
    void redrawCurrentChat();

    void redrawUsers();

private:
    NetClient* m_net;

    // UI
    QListWidget* m_chats = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_createRoomBtn = nullptr;
    QPushButton* m_joinRoomBtn = nullptr;

    QTextEdit* m_log = nullptr;
    QLineEdit* m_text = nullptr;
    QPushButton* m_sendBtn = nullptr;

    // state
    QString m_currentChat; // "@bob" or "#room1"
    QMap<QString, QStringList> m_chatLog;

    QSet<QString> m_joinedRooms; // room names without '#'
    bool m_loadingHistory = false;

    // users
    QSet<QString> m_allUsers;
    QSet<QString> m_onlineUsers;
};
