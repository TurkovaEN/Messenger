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
#include <QListWidgetItem>
#include <QInputDialog>
#include <QDateTime>
#include <QPainter>
#include <QPixmap>
#include <QIcon>

static QIcon makeCircleIcon(const QColor& color) {
    QPixmap pm(12, 12);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(1, 1, 10, 10);
    p.end();

    return QIcon(pm);
}

static QString toHtmlMessageBlock(const QString& s) {
    // expects: "name: text" or "name: text\nDD.MM HH:mm"
    QString esc = s.toHtmlEscaped();
    int nl = esc.indexOf('\n');
    if (nl < 0) return "<div>" + esc + "</div>";

    QString top = esc.left(nl);
    QString bottom = esc.mid(nl + 1);
    return "<div>" + top + "<br><span style='color:gray;font-size:8pt'>" + bottom + "</span></div>";
}

static QListWidgetItem* findItemByChatKey(QListWidget* list, const QString& chatKey) {
    for (int i = 0; i < list->count(); i++) {
        auto* it = list->item(i);
        if (it->data(Qt::UserRole).toString() == chatKey) return it;
    }
    return nullptr;
}

static void setItemBadge(QListWidgetItem* it, const QString& baseText, int unread) {
    if (!it) return;
    if (unread > 0) it->setText(baseText + QString(" (%1)").arg(unread));
    else it->setText(baseText);
}

MainWindow::MainWindow(NetClient* net, QWidget* parent)
    : QMainWindow(parent),
      m_net(net)
{
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout();

    // Buttons row
    auto* btnRow = new QHBoxLayout();
    m_refreshBtn = new QPushButton("Refresh");
    m_createRoomBtn = new QPushButton("Create room");
    m_joinRoomBtn = new QPushButton("Join room");
    m_joinRoomBtn->hide(); // auto-join by click
    btnRow->addWidget(m_refreshBtn);
    btnRow->addWidget(m_createRoomBtn);
    btnRow->addWidget(m_joinRoomBtn);
    btnRow->addStretch(1);
    root->addLayout(btnRow);

    // Main area
    auto* mainRow = new QHBoxLayout();

    m_chats = new QListWidget();
    m_chats->setFixedWidth(180);
    mainRow->addWidget(m_chats, 0);

    auto* rightCol = new QVBoxLayout();

    m_log = new QTextEdit();
    m_log->setReadOnly(true);
    m_log->setAcceptRichText(true);
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
    connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    connect(m_createRoomBtn, &QPushButton::clicked, this, &MainWindow::onCreateRoomClicked);
    connect(m_joinRoomBtn, &QPushButton::clicked, this, &MainWindow::onJoinRoomClicked);
    connect(m_chats, &QListWidget::itemClicked, this, &MainWindow::onChatSelected);

    // NetClient connections
    connect(m_net, &NetClient::disconnected, this, &MainWindow::onNetDisconnected);
    connect(m_net, &NetClient::error, this, &MainWindow::onNetError);
    connect(m_net, &NetClient::message, this, &MainWindow::onNetMessage);

    connect(m_net, &NetClient::messageForChat, this, &MainWindow::onChatMessage);
    connect(m_net, &NetClient::historyItem, this, &MainWindow::onHistoryItem);
    connect(m_net, &NetClient::historyEnd, this, &MainWindow::onHistoryEnd);

    // Users list: online
    connect(m_net, &NetClient::usersList, this, [this](const QStringList& users){
        m_onlineUsers.clear();
        for (const QString& u : users) {
            QString uu = u.trimmed();
            if (!uu.isEmpty()) m_onlineUsers.insert(uu);
        }
        redrawUsers();
    });

    // Users list: all registered
    connect(m_net, &NetClient::usersAllList, this, [this](const QStringList& users){
        m_allUsers.clear();
        for (const QString& u : users) {
            QString uu = u.trimmed();
            if (!uu.isEmpty()) m_allUsers.insert(uu);
        }
        redrawUsers();
    });

    // Rooms list
    connect(m_net, &NetClient::roomsList, this, [this](const QStringList& rooms){
        // remove old # items
        for (int i = m_chats->count() - 1; i >= 0; --i) {
            if (m_chats->item(i)->data(Qt::UserRole).toString().startsWith("#"))
                delete m_chats->takeItem(i);
        }

        const QIcon joinedIcon = makeCircleIcon(QColor(70, 130, 255)); // blue
        const QIcon notJoinedIcon = makeCircleIcon(QColor(150, 150, 150)); // gray

        for (const QString& r : rooms) {
            QString rr = r.trimmed();
            if (rr.isEmpty()) continue;

            QString key = "#" + rr;

            auto* item = new QListWidgetItem();
            item->setData(Qt::UserRole, key);

            if (m_joinedRooms.contains(rr)) item->setIcon(joinedIcon);
            else item->setIcon(notJoinedIcon);

            int unread = m_unread.value(key, 0);
            setItemBadge(item, key, unread);
            if (unread > 0) {
                QFont f = item->font();
                f.setBold(true);
                item->setFont(f);
            }

            m_chats->addItem(item);
        }
    });

    setUiEnabled(true);
    onRefreshClicked();
}

MainWindow::~MainWindow() = default;

void MainWindow::setUiEnabled(bool connected) {
    m_chats->setEnabled(connected);
    m_refreshBtn->setEnabled(connected);
    m_createRoomBtn->setEnabled(connected);
    m_text->setEnabled(connected);
    m_sendBtn->setEnabled(connected);
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
    // not used (auto-join), keep for future
    bool ok = false;
    QString room = QInputDialog::getText(this, "Join room", "Room name:", QLineEdit::Normal, "", &ok).trimmed();
    if (!ok || room.isEmpty()) return;
    m_net->joinRoom(room);
    m_joinedRooms.insert(room);
    onRefreshClicked();
}

void MainWindow::onChatSelected(QListWidgetItem* item) {
    if (!item) return;

    QString key = item->data(Qt::UserRole).toString();
    if (!key.isEmpty()) m_currentChat = key;
    else m_currentChat = item->text();

    m_chatOpenedTs = QDateTime::currentSecsSinceEpoch();
    m_newSeparatorShown = false;
    // remember unread state BEFORE we reset unread counter
m_showNewAfterHistory = (m_unread.value(m_currentChat, 0) > 0);

    // mark as read
    m_unread[m_currentChat] = 0;
    setItemBadge(item, m_currentChat, 0);
    QFont f = item->font();
    f.setBold(false);
    item->setFont(f);

    // auto-join room
    if (m_currentChat.startsWith("#")) {
        QString room = m_currentChat.mid(1);
        if (!m_joinedRooms.contains(room)) {
            m_log->append("[ui] joining room ...");
            m_net->joinRoom(room);
            m_joinedRooms.insert(room);
            onRefreshClicked();
        }
    }

    // load history
    m_loadingHistory = true;
    m_chatLog[m_currentChat].clear();
    m_log->clear();
    m_log->append("[history] loading...");

    if (m_currentChat.startsWith("@")) {
        QString peer = m_currentChat.mid(1);
        m_net->requestHistoryDm(peer, 50);
        return;
    }
    if (m_currentChat.startsWith("#")) {
        QString room = m_currentChat.mid(1);
        m_net->requestHistoryRoom(room, 50);
        return;
    }
}

void MainWindow::redrawCurrentChat() {
    m_log->clear();
    if (m_currentChat.isEmpty()) return;

    const auto lines = m_chatLog.value(m_currentChat);
    for (const QString& s : lines) {
        m_log->append(toHtmlMessageBlock(s));
    }
}

void MainWindow::onSendClicked() {
    if (m_currentChat.isEmpty()) {
        m_log->append("[error] select chat on the left");
        return;
    }
    const QString text = m_text->text();
    if (text.isEmpty()) return;

    QString t = QDateTime::currentDateTime().toString("dd.MM HH:mm");
    QString myLine = QString("me: %1  ✓\n%2").arg(text, t);

    if (m_currentChat.startsWith("@")) {
        QString to = m_currentChat.mid(1);
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

    m_text->clear();
    redrawCurrentChat();
}

void MainWindow::onNetDisconnected() {
    m_log->append("[net] disconnected");
    setUiEnabled(false);
}

void MainWindow::onNetError(const QString& msg) {
    m_log->append("[error] " + msg);
}

void MainWindow::onNetMessage(const QString& msg) {
    // keep only useful messages
    if (msg.startsWith("[info]") || msg.startsWith("[error]")) {
        m_log->append(msg.toHtmlEscaped());
    }
}

void MainWindow::onChatMessage(const QString& chatKey, const QString& line, qint64 ts) {
   if (m_currentChat == chatKey && !m_loadingHistory) {
    if (!m_newSeparatorShown && ts > 0 && ts >= m_chatOpenedTs) {
        m_newSeparatorShown = true;
        m_chatLog[chatKey].append("--- NEW ---");
        m_log->append(toHtmlMessageBlock("--- NEW ---"));
    }
}
    m_chatLog[chatKey].append(line);

    // unread
    if (m_currentChat != chatKey) {
        m_unread[chatKey] = m_unread.value(chatKey, 0) + 1;

        auto* it = findItemByChatKey(m_chats, chatKey);
        if (it) {
            setItemBadge(it, chatKey, m_unread[chatKey]);
            QFont f = it->font();
            f.setBold(true);
            it->setFont(f);
        }
    }

    if (m_currentChat == chatKey && !m_loadingHistory) {
        m_log->append(toHtmlMessageBlock(line));
    }
}

void MainWindow::onHistoryItem(const QString& chatKey, const QString& line) {
    m_chatLog[chatKey].append(line);
    if (m_currentChat == chatKey) {
        if (m_loadingHistory && m_log->toPlainText() == "[history] loading...") {
            m_log->clear();
        }
        m_log->append(toHtmlMessageBlock(line));
    }
}

void MainWindow::onHistoryEnd(const QString& chatKey) {
    if (m_currentChat == chatKey) {
        m_loadingHistory = false;

        if (m_chatLog[chatKey].isEmpty()) {
            m_log->clear();
            m_log->append("[history] empty");
        }

        // show NEW separator once after loading history if this chat had unread messages
        if (m_showNewAfterHistory) {
            m_showNewAfterHistory = false;
            //m_chatLog[chatKey].append("--- NEW ---");
            //m_log->append(toHtmlMessageBlock("--- NEW ---"));
        }
    }
}

void MainWindow::redrawUsers() {
    // remove old @ items
    for (int i = m_chats->count() - 1; i >= 0; --i) {
        if (m_chats->item(i)->data(Qt::UserRole).toString().startsWith("@"))
            delete m_chats->takeItem(i);
    }

    const QIcon onlineIcon = makeCircleIcon(QColor(0, 200, 0));      // green
    const QIcon offlineIcon = makeCircleIcon(QColor(150, 150, 150)); // gray

    QStringList all = QStringList(m_allUsers.begin(), m_allUsers.end());
    all.sort(Qt::CaseInsensitive);

    for (const QString& u : all) {
        QString key = "@" + u;

        auto* item = new QListWidgetItem();
        item->setData(Qt::UserRole, key);

        if (m_onlineUsers.contains(u)) item->setIcon(onlineIcon);
        else item->setIcon(offlineIcon);

        int unread = m_unread.value(key, 0);
        setItemBadge(item, key, unread);
        if (unread > 0) {
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        }

        m_chats->addItem(item);
    }
}
