#pragma once

#include <QMainWindow>
#include <QMap>
#include <QStringList>
#include <QSet>

class QLineEdit;
class QPushButton;
class QTextEdit;
class QCheckBox;
class QListWidget;
class QListWidgetItem;

class NetClient;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onSendClicked();

    void onNetConnected();
    void onNetDisconnected();
    void onNetError(const QString& msg);
    void onNetMessage(const QString& msg);
void onRefreshClicked();
void onCreateRoomClicked();
void onJoinRoomClicked();
void onChatSelected(QListWidgetItem* item);
void onChatMessage(const QString& chatKey, const QString& line);
void onHistoryItem(const QString& chatKey, const QString& line);
void onHistoryEnd(const QString& chatKey);

private:
    void setUiEnabled(bool connected);
    bool m_loadingHistory = false;

    NetClient* m_net;

    QLineEdit* m_host;
    QLineEdit* m_port;
    QLineEdit* m_user;
    QPushButton* m_connectBtn;
    QCheckBox* m_register;

    QTextEdit* m_log;

    QLineEdit* m_to;
    QLineEdit* m_text;
    QPushButton* m_sendBtn;

    QListWidget* m_chats;
QPushButton* m_refreshBtn;
QPushButton* m_createRoomBtn;
QPushButton* m_joinRoomBtn;
QString m_currentChat; // "@bob" or "#room1"
QMap<QString, QStringList> m_chatLog;
void redrawCurrentChat();
QSet<QString> m_joinedRooms; // stores room names without '#'
QSet<QString> m_allUsers;
QSet<QString> m_onlineUsers;
void redrawUsers();
};
