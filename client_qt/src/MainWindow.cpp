#include "MainWindow.h"
#include "NetClient.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QLabel>
#include <QListWidget>
#include <QCheckBox>
#include <QInputDialog>


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_net(new NetClient(this))
{
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout();

    // Connection row
    auto* connRow = new QHBoxLayout();
    m_host = new QLineEdit("127.0.0.1");
    m_port = new QLineEdit("5555");
    m_user = new QLineEdit("alice");
    m_register = new QCheckBox("Register");
    m_connectBtn = new QPushButton("Connect");

    connRow->addWidget(new QLabel("Host:"));
    connRow->addWidget(m_host);
    connRow->addWidget(new QLabel("Port:"));
    connRow->addWidget(m_port);
    connRow->addWidget(new QLabel("User:"));
    connRow->addWidget(m_user);
    connRow->addWidget(m_register);
    connRow->addWidget(m_connectBtn);
    root->addLayout(connRow);

    // Buttons row
    auto* btnRow = new QHBoxLayout();
    m_refreshBtn = new QPushButton("Refresh");
    m_createRoomBtn = new QPushButton("Create room");
    m_joinRoomBtn = new QPushButton("Join room");
    btnRow->addWidget(m_refreshBtn);
    btnRow->addWidget(m_createRoomBtn);
    btnRow->addWidget(m_joinRoomBtn);
    btnRow->addStretch(1);
    root->addLayout(btnRow);

    // Main area: chats list + log + send
    auto* mainRow = new QHBoxLayout();

    m_chats = new QListWidget();
    m_chats->setMinimumWidth(100);
    mainRow->addWidget(m_chats, 0);

    auto* rightCol = new QVBoxLayout();

    m_log = new QTextEdit();
    m_log->setReadOnly(true);
    rightCol->addWidget(m_log);

    auto* sendRow = new QHBoxLayout();
    m_text = new QLineEdit();
    m_sendBtn = new QPushButton("Send");
    sendRow->addWidget(new QLabel("Text:"));
    sendRow->addWidget(m_text);
    sendRow->addWidget(m_sendBtn);
    rightCol->addLayout(sendRow);

    mainRow->addLayout(rightCol, 1);
    root->addLayout(mainRow);

    central->setLayout(root);
    setCentralWidget(central);

    // UI connections
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    connect(m_createRoomBtn, &QPushButton::clicked, this, &MainWindow::onCreateRoomClicked);
    connect(m_joinRoomBtn, &QPushButton::clicked, this, &MainWindow::onJoinRoomClicked);
    connect(m_chats, &QListWidget::itemClicked, this, &MainWindow::onChatSelected);


    // NetClient connections
    connect(m_net, &NetClient::connected, this, &MainWindow::onNetConnected);
    connect(m_net, &NetClient::disconnected, this, &MainWindow::onNetDisconnected);
    connect(m_net, &NetClient::error, this, &MainWindow::onNetError);
    connect(m_net, &NetClient::message, this, &MainWindow::onNetMessage);
     connect(m_net, &NetClient::messageForChat, this, &MainWindow::onChatMessage);
     connect(m_net, &NetClient::historyItem, this, &MainWindow::onHistoryItem);
connect(m_net, &NetClient::historyEnd, this, &MainWindow::onHistoryEnd);

    // Fill chats list
    connect(m_net, &NetClient::usersList, this, [this](const QStringList& users){
    m_onlineUsers.clear();
    for (const QString& u : users) {
        QString uu = u.trimmed();
        if (!uu.isEmpty()) m_onlineUsers.insert(uu);
    }
    redrawUsers();
});

connect(m_net, &NetClient::usersAllList, this, [this](const QStringList& users){
    m_allUsers.clear();
    for (const QString& u : users) {
        QString uu = u.trimmed();
        if (!uu.isEmpty()) m_allUsers.insert(uu);
    }
    redrawUsers();
});

    connect(m_net, &NetClient::roomsList, this, [this](const QStringList& rooms){
        // remove old # items
        for (int i = m_chats->count() - 1; i >= 0; --i) {
            if (m_chats->item(i)->text().startsWith("#"))
                delete m_chats->takeItem(i);
        }
        for (const QString& r : rooms) {
            QString rr = r.trimmed();
            if (rr.isEmpty()) continue;
            m_chats->addItem("#" + rr);
        }
    });

    setUiEnabled(false);
}

MainWindow::~MainWindow() = default;

void MainWindow::setUiEnabled(bool connected) {
    m_chats->setEnabled(connected);
    m_refreshBtn->setEnabled(connected);
    m_createRoomBtn->setEnabled(connected);
    m_joinRoomBtn->setEnabled(connected);
    m_text->setEnabled(connected);
    m_sendBtn->setEnabled(connected);
}

void MainWindow::onConnectClicked() {
    const QString host = m_host->text().trimmed();
    const quint16 port = (quint16)m_port->text().toUShort();
    const QString user = m_user->text().trimmed();

    m_log->append(QString("Connecting to %1:%2 as %3...").arg(host).arg(port).arg(user));
    m_net->connectTo(host, port, user, m_register->isChecked());
}

void MainWindow::onRefreshClicked() {
    m_net->requestUsersAll();
    m_net->requestUsers();   // online
    m_net->requestRooms();
}

void MainWindow::onCreateRoomClicked() {
    bool ok = false;
    QString room = QInputDialog::getText(this, "Create room", "Room name:", QLineEdit::Normal, "", &ok).trimmed();
    if (!ok || room.isEmpty()) return;
    m_net->createRoom(room);
    m_net->requestRooms();
}

void MainWindow::onJoinRoomClicked() {
    bool ok = false;
    QString room = QInputDialog::getText(this, "Join room", "Room name:", QLineEdit::Normal, "", &ok).trimmed();
    if (!ok || room.isEmpty()) return;
    m_net->joinRoom(room);
}

void MainWindow::onChatSelected(QListWidgetItem* item) {
    if (!item) return;
    m_currentChat = item->text();

    if (m_currentChat.startsWith("@")) {
    int sp = m_currentChat.indexOf(' ');
    if (sp >= 0) m_currentChat = m_currentChat.left(sp);
}

    if (m_currentChat.startsWith("#")) {
        QString room = m_currentChat.mid(1);
        if (!m_joinedRooms.contains(room)) {
            m_log->append(QString("[ui] joining #%1 ...").arg(room));
            m_net->joinRoom(room);
            m_joinedRooms.insert(room);
        }
    }
    // load history for selected chat
    m_loadingHistory = true;
    m_chatLog[m_currentChat].clear();
    m_log->clear();
    m_log->append("[history] loading...");

    if (m_currentChat.startsWith("@")) {
        QString peer = m_currentChat.mid(1);
        m_net->requestHistoryDm(peer, 50);
    } else if (m_currentChat.startsWith("#")) {
        QString room = m_currentChat.mid(1);
        m_net->requestHistoryRoom(room, 50);
    }
    return;
}

void MainWindow::onSendClicked() {
    if (m_currentChat.isEmpty()) {
        m_log->append("[error] select chat on the left");
        return;
    }
    const QString text = m_text->text();
    if (text.isEmpty()) return;

    QString myLine = QString("me: %1").arg(text);

if (m_currentChat.startsWith("@")) {
    QString to = m_currentChat.mid(1);
    int sp = to.indexOf(' ');
    if (sp >= 0) to = to.left(sp);
        m_net->sendDm(to, text);
        m_chatLog[m_currentChat].append(myLine);
    } else if (m_currentChat.startsWith("#")) {
        QString room = m_currentChat.mid(1);
        m_net->sendRoom(room, text);
        m_chatLog[m_currentChat].append(myLine);
    } else {
        m_log->append("[error] unknown chat type");
        return;
    }

    // show immediately
    m_log->append(myLine);
    m_text->clear();
}

void MainWindow::onNetConnected() {
    m_log->append("[net] login ok");
    setUiEnabled(true);
    onRefreshClicked();
}

void MainWindow::onNetDisconnected() {
    m_log->append("[net] disconnected");
    setUiEnabled(false);
}

void MainWindow::onNetError(const QString& msg) {
    m_log->append("[error] " + msg);
}

void MainWindow::onNetMessage(const QString& msg) {
    m_log->append(msg);
}

void MainWindow::redrawCurrentChat() {
    m_log->clear();
    if (m_currentChat.isEmpty()) return;

    const auto lines = m_chatLog.value(m_currentChat);
    for (const QString& s : lines) {
        m_log->append(s);
    }
}

void MainWindow::onChatMessage(const QString& chatKey, const QString& line) {
    m_chatLog[chatKey].append(line);

    // if user is currently viewing this chat, update UI
    if (m_currentChat == chatKey) {
        m_log->append(line);
    }
}

void MainWindow::onHistoryItem(const QString& chatKey, const QString& line) {
    m_chatLog[chatKey].append(line);
    if (m_currentChat == chatKey) {
        // пока грузим, не перерисовываем каждый раз полностью
        // просто добавляем строку
        if (m_loadingHistory) {
            if (m_log->toPlainText() == "[history] loading...") m_log->clear();
        }
        m_log->append(line);
    }
}

void MainWindow::onHistoryEnd(const QString& chatKey) {
    if (m_currentChat == chatKey) {
        m_loadingHistory = false;
        // если история пустая
        if (m_chatLog[chatKey].isEmpty()) {
            m_log->clear();
            m_log->append("[history] empty");
        }
    }
}

void MainWindow::redrawUsers() {
    // remove old @ items
    for (int i = m_chats->count() - 1; i >= 0; --i) {
        if (m_chats->item(i)->text().startsWith("@"))
            delete m_chats->takeItem(i);
    }

    QStringList all = QStringList(m_allUsers.begin(), m_allUsers.end());
    all.sort(Qt::CaseInsensitive);

    for (const QString& u : all) {
        QString visible = "@" + u;
        if (m_onlineUsers.contains(u)) visible += " (online)";
        m_chats->addItem(visible);
    }
}
